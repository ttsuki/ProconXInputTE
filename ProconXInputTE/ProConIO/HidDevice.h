#pragma once

#include <array>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>

#include <hidapi.h>

namespace ProconXInputTE
{
	struct Buffer
	{
		std::array<uint8_t, 1024> buffer_{};
		size_t size_{};

		Buffer() = default;
		Buffer(std::initializer_list<uint8_t> list);
		Buffer(const Buffer& other) = default;
		Buffer& operator=(const Buffer& other) = default;

		uint8_t* data() noexcept { return buffer_.data(); }
		const uint8_t* data() const noexcept { return buffer_.data(); }
		size_t size() const noexcept { return size_; }
		size_t capacity() const noexcept { return buffer_.size(); }
		void resize(size_t sz) noexcept { size_ = sz; }
		uint8_t& operator [](size_t index) { return buffer_[index]; }
		const uint8_t& operator [](size_t index) const { return buffer_[index]; }
		Buffer& operator +=(const Buffer& data);
	};

	class HidDevice
	{
		hid_device* dev_{};

	public:
		HidDevice() = default;
		HidDevice(const HidDevice& other) = delete;
		HidDevice& operator=(const HidDevice& other) = delete;
		virtual ~HidDevice();

		bool OpenDevice(const char* path);
		void CloseDevice();

		int SendPacket(const Buffer& data);
		Buffer ReceivePacket(int millisec = 20);
	};

	class HidDeviceThreaded
	{
		HidDevice device_{};
		std::thread receiverThread_{};
		std::atomic_flag running_{ ATOMIC_FLAG_INIT };
		std::function<void(const Buffer& data)> packetCallback_{};
		std::mutex packetCallbackMutex_;

	public:
		HidDeviceThreaded() = default;
		HidDeviceThreaded(const HidDeviceThreaded& other) = delete;
		HidDeviceThreaded& operator=(const HidDeviceThreaded& other) = delete;
		virtual ~HidDeviceThreaded();

		bool OpenDevice(const char* path);
		void CloseDevice();

		int SendPacket(const Buffer& data);
		void ExchangePacket(const Buffer& send, const std::function<bool(const Buffer& received)>& wait);

		/// Please override me in derived class.
		virtual void OnPacket(const Buffer&)
		{
		}
	};
}
