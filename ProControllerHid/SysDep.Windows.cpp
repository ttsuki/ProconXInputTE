#include "SysDep.h"

#define WIN32_LEAN_AND_MEANS
#include <Windows.h>
#include <string>
#include <algorithm>

#ifndef _WIN32
#define USE_HIDAPI
#endif

#ifdef USE_HIDAPI
#  include <hidapi.h>
#else
#  include <SetupAPI.h>
#  include <hidsdi.h>
#  pragma comment(lib,"hid.lib")
#  pragma comment(lib,"setupapi.lib")
#endif


namespace ProControllerHid
{
	namespace SysDep
	{
		void SetThreadPriorityToRealtime()
		{
			::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		}

		void SetThreadName(const char *threadName)
		{
			SetThreadDescription(::GetCurrentThread(),
				std::wstring(threadName, threadName + strlen(threadName)).c_str());
		}

		HidDevice* HidDevice::OpenHidDevice(const char *devicePath)
		{
#ifdef USE_HIDAPI

			class HidDeviceImpl : public HidDevice
			{
			public:
				hid_device *device_{};

				HidDeviceImpl()
				{
				}

				~HidDeviceImpl() override
				{
					hid_close(device_);
					device_ = nullptr;
				}

				bool Open(const char *devicePath)
				{
					hid_device *dev = hid_open_path(devicePath);
					if (dev)
					{
						device_ = dev;
					}
					return dev;
				}

				void Close() override
				{
					delete this;
				}

				int SendPacket(const void *data, size_t length) override
				{
					return hid_write(device_, static_cast<const unsigned char*>(data), length);
				}

				int ReceivePacket(void *buffer, size_t length, int timeoutMillisec) override
				{
					return hid_read_timeout(device_, static_cast<unsigned char*>(buffer), length, timeoutMillisec);
				}
			};

#else

			class HidDeviceImpl : public HidDevice
			{
			public:
				HANDLE device_{INVALID_HANDLE_VALUE};
				std::vector<uint8_t> inputBuffer_{};
				std::vector<uint8_t> outputBuffer_{};

				OVERLAPPED readOperation_{};
				bool readOperationIncomplete_{};
				HANDLE readOperationCompleteEvent_{};

				HidDeviceImpl()
				{
					readOperationCompleteEvent_ = CreateEvent(nullptr, false, false, nullptr);
				}

				~HidDeviceImpl() override
				{
					if (device_ != INVALID_HANDLE_VALUE)
					{
						CancelIo(device_);
						CloseHandle(device_);
					}

					CloseHandle(readOperationCompleteEvent_);
				}

				bool Open(const char *devicePath)
				{
					HANDLE device = CreateFileA(
						devicePath,
						GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

					if (device == INVALID_HANDLE_VALUE)
					{
						return false;
					}

					if (!HidD_SetNumInputBuffers(device, 64))
					{
						return false;
					}

					PHIDP_PREPARSED_DATA preparsedData = nullptr;
					if (!HidD_GetPreparsedData(device, &preparsedData))
					{
						return false;
					}

					HIDP_CAPS caps{};
					HidP_GetCaps(preparsedData, &caps);
					inputBuffer_.resize(caps.InputReportByteLength);
					outputBuffer_.resize(caps.OutputReportByteLength);
					HidD_FreePreparsedData(preparsedData);

					device_ = device;
					return true;
				}

				void Close() override
				{
					delete this;
				}

				int SendPacket(const void *data, size_t length) override
				{
					// data is too short?
					if (length < outputBuffer_.size())
					{
						memcpy(outputBuffer_.data(), data, length);
						memset(outputBuffer_.data() + length, 0, outputBuffer_.size() - length);
						data = outputBuffer_.data();
						length = outputBuffer_.size();
					}

					OVERLAPPED ol{};
					auto r = WriteFile(device_, data, length, nullptr, &ol);
					if (!r && GetLastError() != ERROR_IO_PENDING)
					{
						// failed.
						return -1;
					}

					DWORD wrote{};
					auto r2 = GetOverlappedResult(device_, &ol, &wrote, true);
					if (!r2)
					{
						// failed.
						return -2;
					}

					return static_cast<int>(wrote);
				}

				int ReceivePacket(void *buffer, size_t length, int timeoutMillisec) override
				{
					if (!readOperationIncomplete_)
					{
						readOperationIncomplete_ = true;
						memset(inputBuffer_.data(), 0, inputBuffer_.size());
						ResetEvent(readOperationCompleteEvent_);

						readOperation_ = {};
						readOperation_.hEvent = readOperationCompleteEvent_;

						auto r = ReadFile(device_, inputBuffer_.data(), inputBuffer_.size(), nullptr, &readOperation_);

						if (!r && GetLastError() != ERROR_IO_PENDING)
						{
							// failed.
							CancelIo(device_);
							readOperationIncomplete_ = false;
							return -1;
						}
					}

					auto r2 = WaitForSingleObject(readOperationCompleteEvent_, std::max(0, timeoutMillisec));
					if (r2 == WAIT_TIMEOUT)
					{
						// continue read operation in background.
						return 0;
					}

					if (r2 != WAIT_OBJECT_0)
					{
						// failed.
						CancelIo(device_);
						readOperationIncomplete_ = false;
						return -1;
					}
					readOperationIncomplete_ = false;

					DWORD read{};
					auto r3 = GetOverlappedResult(device_, &readOperation_, &read, true);
					if (!r3)
					{
						// failed.
						return -1;
					}

					int ret = 0;
					if (read > 0)
					{
						const decltype(inputBuffer_)::value_type *src = inputBuffer_.data();

						if (inputBuffer_[0] == 0x00)
						{
							src++;
							read--;
						}

						ret = std::min<size_t>(read, length);
						memcpy(buffer, src, ret);
					}
					return ret;
				}
			};
			
#endif

			HidDeviceImpl *impl = new HidDeviceImpl();
			if (!impl->Open(devicePath))
			{
				impl->Close();
				impl = nullptr;
			}
			return impl;
		}

		std::vector<HidDeviceInfo> EnumerateAllConnectedHidDevicePaths(int vendorId, int productId)
		{
			std::vector<HidDeviceInfo> result;

#ifdef USE_HIDAPI

			auto *devs = hid_enumerate(vendorId, productId);
			if (devs)
			{
				for (auto dev = devs; dev; dev = dev->next)
				{
					HidDeviceInfo device{};
					device.devicePath = dev->path;
					device.manufactureName = dev->manufacturer_string;
					device.productName = dev->product_string;
					device.serialNumber = dev->serial_number;
					result.push_back(device);
				}
				hid_free_enumeration(devs);
			}

#else

			GUID hidGuid{};
			HidD_GetHidGuid(&hidGuid);

			HDEVINFO hDevInfo = SetupDiGetClassDevsA(&hidGuid, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

			SP_DEVICE_INTERFACE_DATA id = {sizeof(SP_DEVICE_INTERFACE_DATA)};

			for (int index = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &hidGuid, index, &id); index++)
			{
				DWORD size = 0;

				// Get Device Path
				std::vector<uint8_t> detailDataBuffer(256 + 16);
				auto pDet = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A *>(detailDataBuffer.data());
				pDet->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
				if (!SetupDiGetDeviceInterfaceDetailA(hDevInfo, &id, pDet, detailDataBuffer.size(), &size, nullptr))
				{
					continue;
				}

				HANDLE handle = CreateFileA(
					pDet->DevicePath,
					0, FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 0, 0);

				if (handle == INVALID_HANDLE_VALUE)
				{
					continue;
				}

				HIDD_ATTRIBUTES attr{};
				if (!HidD_GetAttributes(handle, &attr)
					|| attr.VendorID != vendorId
					|| attr.ProductID != productId)
				{
					CloseHandle(handle);
					continue;
				}

				std::vector<wchar_t> buffer(256);

				HidDeviceInfo device{};
				device.devicePath = pDet->DevicePath;

				if (HidD_GetManufacturerString(handle, buffer.data(), buffer.size() * sizeof(wchar_t)))
				{
					device.manufactureName = buffer.data();
				}

				if (HidD_GetProductString(handle, buffer.data(), buffer.size() * sizeof(wchar_t)))
				{
					device.productName = buffer.data();
				}

				if (HidD_GetSerialNumberString(handle, buffer.data(), buffer.size() * sizeof(wchar_t)))
				{
					device.serialNumber = buffer.data();
				}

				CloseHandle(handle);
				result.push_back(device);
			}

			SetupDiDestroyDeviceInfoList(hDevInfo);

#endif
			return result;
		}
	}
}
