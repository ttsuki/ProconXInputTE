#include "SysDep.h"

#define WIN32_LEAN_AND_MEANS
#include <Windows.h>
#include <string>

#include <hidapi.h>

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
			class HidDeviceImpl : public HidDevice
			{
			public:
				hid_device *device_{};

				HidDeviceImpl(hid_device *device)
					: device_(device)
				{
				}

				~HidDeviceImpl() override
				{
					hid_close(device_);
					device_ = nullptr;
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

			hid_device *dev = hid_open_path(devicePath);
			if (!dev)
			{
				return nullptr;
			}

			return new HidDeviceImpl(dev);
		}

		std::vector<HidDeviceInfo> EnumerateAllConnectedHidDevicePaths(int vendorId, int productId)
		{
			std::vector<HidDeviceInfo> result;
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

			return result;
		}
	}
}
