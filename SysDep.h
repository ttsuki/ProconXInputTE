#pragma once

#include <vector>
#include <string>

namespace ProControllerHid
{
	namespace SysDep
	{
		void SetThreadPriorityToRealtime();
		void SetThreadName(const char *threadName);

		struct HidDevice
		{
			static HidDevice* OpenHidDevice(const char *devicePath);
			HidDevice() = default;
			HidDevice(const HidDevice &other) = delete;
			HidDevice(HidDevice &&other) noexcept = delete;
			HidDevice& operator=(const HidDevice &other) = delete;
			HidDevice& operator=(HidDevice &&other) noexcept = delete;
			virtual ~HidDevice() = default;

			virtual void Close() = 0;
			virtual int SendPacket(const void *data, size_t length) = 0;
			virtual int ReceivePacket(void *buffer, size_t length, int timeoutMillisec) = 0;
		};

		struct HidDeviceInfo
		{
			std::string devicePath;
			std::wstring manufactureName;
			std::wstring productName;
			std::wstring serialNumber;
		};

		std::vector<HidDeviceInfo> EnumerateAllConnectedHidDevicePaths(int vendorId, int productId);
	}
}
