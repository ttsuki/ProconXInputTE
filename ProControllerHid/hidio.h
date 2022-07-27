#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace hidio
{
	struct device_info
	{
		std::string device_path;
		std::wstring manufacture_string;
		std::wstring product_string;
		std::wstring serial_number;
	};

	std::vector<device_info> enumerate_devices(uint16_t vendorId, uint16_t productId);

	struct device
	{
		// open device
		static std::unique_ptr<device> open(const char* device_path);

		// returns device path.
		[[nodiscard]] virtual const char* device_path() const = 0;

		// returns written bytes, or -1 on error.
		[[nodiscard]] virtual int write(const void* data, size_t length) = 0;

		// returns read bytes, or -1 on error.
		[[nodiscard]] virtual int read(void* buffer, size_t length, int timeout_milliseconds = -1) = 0;

		// close device.
		virtual ~device() = default;
	};

	void set_thread_priority_to_realtime();
}
