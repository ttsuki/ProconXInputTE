#include "hidio.h"

#include <cstdint>
#include <cassert>

#include <type_traits>
#include <memory>
#include <vector>
#include <string>
#include <functional>

//#define WITH_HIDAPI

#if !defined(_WIN32) || defined(WITH_HIDAPI)

#include <hidapi/hidapi.h>
#pragma comment(lib, "hidapi.lib")

namespace hidio
{
	std::vector<device_info> enumerate_devices(uint16_t vendorId, uint16_t productId)
	{
		std::vector<device_info> result;

		if (auto* device_list = ::hid_enumerate(vendorId, productId))
		{
			for (auto i = device_list; i; i = i->next)
			{
				device_info t{};
				t.device_path = i->path;
				t.manufacture_string = i->manufacturer_string;
				t.product_string = i->product_string;
				t.serial_number = i->serial_number;
				result.push_back(t);
			}
			hid_free_enumeration(device_list);
		}

		return result;
	}

	std::unique_ptr<device> device::open(const char* device_path)
	{
		struct impl final : device
		{
			std::string device_name_{};
			hid_device* device_{};
			impl(const char* device_path) : device_name_(device_path), device_(hid_open_path(device_path)) { }
			[[nodiscard]] bool is_open() const { return device_; }
			[[nodiscard]] const char* device_path() const override { return device_name_.c_str(); }
			[[nodiscard]] int write(const void* data, size_t length) override { return hid_write(device_, static_cast<const unsigned char*>(data), length); }
			[[nodiscard]] int read(void* buffer, size_t length, int timeout_milliseconds) override { return hid_read_timeout(device_, static_cast<unsigned char*>(buffer), length, timeout_milliseconds); }
			~impl() override { hid_close(device_); }
		};

		if (auto d = std::make_unique<impl>(device_path); d->is_open())
			return std::move(d);
		else
			return nullptr;
	}

	void set_thread_priority_realtime()
	{
	}
}

#else

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <SetupAPI.h>
#include <hidsdi.h>
#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

namespace hidio
{
	std::vector<device_info> enumerate_devices(uint16_t vendorId, uint16_t productId)
	{
		std::vector<device_info> result;
		constexpr GUID GUID_DEVINTERFACE_HID = {0x4D1E55B2L, 0xF16F, 0x11CF, {0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

		auto dev_info = std::shared_ptr<std::remove_pointer_t<HDEVINFO>>(
			SetupDiGetClassDevsA(&GUID_DEVINTERFACE_HID, nullptr, nullptr, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT),
			&SetupDiDestroyDeviceInfoList);

		SP_DEVICE_INTERFACE_DATA id{};
		id.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

		for (DWORD index = 0;
		     SetupDiEnumDeviceInterfaces(dev_info.get(), nullptr, &GUID_DEVINTERFACE_HID, index, &id);
		     index++)
		{
			DWORD size = 0;

			// Get Device Path
			uint8_t buffer[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A) + 256]{};
			auto detail_data = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(buffer);
			detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

			if (!SetupDiGetDeviceInterfaceDetailA(dev_info.get(), &id, detail_data, sizeof(buffer), &size, nullptr))
			{
				continue;
			}

			// Open Device
			auto device_handle = std::shared_ptr<std::remove_pointer_t<HANDLE>>(
				CreateFileA(
					detail_data->DevicePath,
					0, FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL, OPEN_EXISTING, 0, 0),
				&CloseHandle);

			if (device_handle.get() == INVALID_HANDLE_VALUE)
			{
				continue;
			}

			// Filter by vid/pid.
			HIDD_ATTRIBUTES attr{};
			if (!HidD_GetAttributes(device_handle.get(), &attr)
				|| attr.VendorID != vendorId
				|| attr.ProductID != productId)
			{
				continue;
			}

			// Collect Device Info
			device_info t{};

			t.device_path = detail_data->DevicePath;

			if (wchar_t buf[256]{};
				HidD_GetManufacturerString(device_handle.get(), buf, sizeof(buf)))
			{
				t.manufacture_string = buf;
			}

			if (wchar_t buf[256]{};
				HidD_GetProductString(device_handle.get(), buf, sizeof(buf)))
			{
				t.product_string = buf;
			}

			if (wchar_t buf[256]{};
				HidD_GetSerialNumberString(device_handle.get(), buf, sizeof(buf)))
			{
				t.serial_number = buf;
			}

			result.push_back(t);
		}

		return result;
	}

	std::unique_ptr<device> device::open(const char* device_path)
	{
		struct impl final : device
		{
			using win32_handle = std::unique_ptr<std::remove_pointer_t<HANDLE>, decltype(&CloseHandle)>;

			std::string device_path_{};
			win32_handle device_{nullptr, &CloseHandle};

			OVERLAPPED reading_overlapped_{};
			bool previous_read_is_incomplete_ = false;
			win32_handle read_operation_completed_{CreateEventA(nullptr, true, false, nullptr), &CloseHandle};

			std::vector<uint8_t> inputBuffer_{};
			std::vector<uint8_t> outputBuffer_{};

			static win32_handle open_device(const char* device_path, size_t* input_report_length, size_t* output_report_length, size_t* feature_report_length)
			{
				auto dev = win32_handle(
					CreateFileA(
						device_path,
						GENERIC_WRITE | GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_FLAG_OVERLAPPED,
						NULL), &CloseHandle);

				if (dev.get() == INVALID_HANDLE_VALUE) return {nullptr, CloseHandle};

				if (!HidD_SetNumInputBuffers(dev.get(), 64)) return {nullptr, CloseHandle};

				// Collect device caps
				if (auto data = std::unique_ptr<std::remove_pointer_t<PHIDP_PREPARSED_DATA>, decltype(&HidD_FreePreparsedData)>(
					[&]
					{
						PHIDP_PREPARSED_DATA t = nullptr;
						return HidD_GetPreparsedData(dev.get(), &t) ? t : nullptr;
					}(), &HidD_FreePreparsedData))
				{
					if (HIDP_CAPS caps{}; HidP_GetCaps(data.get(), &caps) == HIDP_STATUS_SUCCESS)
					{
						if (input_report_length) *input_report_length = caps.InputReportByteLength;
						if (output_report_length) *output_report_length = caps.OutputReportByteLength;
						if (feature_report_length) *feature_report_length = caps.FeatureReportByteLength;
					}
					else return {nullptr, CloseHandle};
				}
				else return {nullptr, CloseHandle};

				return std::move(dev);
			}

			impl(const char* device_path)
				: device_path_(device_path)
			{
				size_t input_report_length{};
				size_t output_report_length{};
				size_t feature_report_length{};

				device_ = open_device(device_path, &input_report_length, &output_report_length, &feature_report_length);
				inputBuffer_.resize(input_report_length);
				outputBuffer_.resize(output_report_length);
			}

			[[nodiscard]] bool is_open() const
			{
				return device_.get() != nullptr;
			}

			[[nodiscard]] const char* device_path() const override
			{
				return device_path_.c_str();
			}

			void reset_io()
			{
				CancelIo(device_.get());
				previous_read_is_incomplete_ = false;
				device_.reset();
				device_ = open_device(device_path(), nullptr, nullptr, nullptr);
			}

			[[nodiscard]] int write(const void* data, size_t length) override
			{
				// if data is shorter than minimum length, zero-extend it.
				if (length < outputBuffer_.size())
				{
					memcpy(outputBuffer_.data(), data, length);
					memset(outputBuffer_.data() + length, 0, outputBuffer_.size() - length);
					data = outputBuffer_.data();
					length = outputBuffer_.size();
				}

				OVERLAPPED writing_overlapped_{};
				if (BOOL r = WriteFile(device_.get(), data, static_cast<DWORD>(length), nullptr, &writing_overlapped_);
					r || GetLastError() == ERROR_IO_PENDING)
				{
					/* OK */
				}
				else
				{
					reset_io();
					return -1; // failed.
				}

				DWORD wrote{};
				if (GetOverlappedResult(device_.get(), &writing_overlapped_, &wrote, true))
				{
					return static_cast<int>(wrote); // complete
				}

				reset_io();
				return -1; // failed.
			}

			[[nodiscard]] int read(void* buffer, size_t length, int timeout_milliseconds) override
			{
				if (!previous_read_is_incomplete_)
				{
					previous_read_is_incomplete_ = true;
					memset(inputBuffer_.data(), 0, inputBuffer_.size());
					ResetEvent(read_operation_completed_.get());

					reading_overlapped_ = OVERLAPPED{};
					reading_overlapped_.hEvent = read_operation_completed_.get();

					if (BOOL r = ReadFile(device_.get(), inputBuffer_.data(), static_cast<DWORD>(inputBuffer_.size()), nullptr, &reading_overlapped_);
						r || GetLastError() == ERROR_IO_PENDING)
					{
						/* OK */
					}
					else
					{
						reset_io();
						return -1; // failed.
					}
				}

				// Wait reading operation complete
				if (auto wait_result = WaitForSingleObject(
						read_operation_completed_.get(),
						timeout_milliseconds >= 0 ? timeout_milliseconds : INFINITE);
					wait_result == WAIT_OBJECT_0)
				{
					previous_read_is_incomplete_ = false;

					DWORD read{};
					if (GetOverlappedResult(device_.get(), &reading_overlapped_, &read, true))
					{
						const uint8_t* src = inputBuffer_.data();
						if (*src == 0x00)
						{
							src++;
							read--;
						}

						if (read <= length)
						{
							memcpy(buffer, src, read);
							return static_cast<int>(read); // complete
						}
					}
				}
				else if (wait_result == WAIT_TIMEOUT)
				{
					return 0; // reading operation is continued in background
				}

				reset_io();
				return -1; // failed
			}

			~impl() override
			{
				reset_io();
				device_.reset();
			}
		};

		if (auto d = std::make_unique<impl>(device_path); d->is_open())
			return {std::move(d)};
		else
			return nullptr;
	}

	void set_thread_priority_to_realtime()
	{
		::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
	}
}

#endif
