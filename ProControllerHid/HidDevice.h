#pragma once

#include <array>
#include <mutex>
#include <thread>
#include <functional>
#include <atomic>
#include <queue>

namespace ProControllerHid
{
	namespace HidIo
	{
		class Buffer final
		{
			std::array<uint8_t, 1024> buffer_{};
			size_t size_{};

		public:
			Buffer() = default;
			Buffer(std::initializer_list<uint8_t> list);
			Buffer(const Buffer &other) = default;
			Buffer& operator=(const Buffer &other) = default;
			~Buffer() = default;

			uint8_t* data() noexcept { return buffer_.data(); }
			const uint8_t* data() const noexcept { return buffer_.data(); }
			size_t size() const noexcept { return size_; }
			size_t capacity() const noexcept { return buffer_.size(); }
			void resize(size_t sz) noexcept { size_ = sz; }
			uint8_t& operator [](size_t index) { return buffer_[index]; }
			const uint8_t& operator [](size_t index) const { return buffer_[index]; }
			Buffer& operator +=(const Buffer &data);
		};

		class BufferQueue final
		{
			std::mutex mutex_{};
			std::queue<Buffer> queue_{};
			std::condition_variable signal_{};

		public:
			void Signal(const Buffer &data);
			Buffer Wait();
			void Clear();
		};

		class HidDevice final
		{
			void *dev_{};

		public:
			HidDevice() = default;
			HidDevice(const HidDevice &other) = delete;
			HidDevice& operator=(const HidDevice &other) = delete;
			~HidDevice();

			bool OpenDevice(const char *path);
			void CloseDevice();
			int SendPacket(const Buffer &data);
			Buffer ReceivePacket(int millisec = -1);
		};

		class HidDeviceThreaded final
		{
			HidDevice deviceToSend_{};
			HidDevice deviceToReceive_{};

			BufferQueue senderQueue_{};
			BufferQueue receiveQueue_{};
			bool running_{};

			std::thread senderThread_{};
			std::atomic_flag senderThreadRunning_{ATOMIC_FLAG_INIT};

			std::thread receiverThread_{};
			std::atomic_flag receiverThreadRunning_{ATOMIC_FLAG_INIT};

			std::thread dispatcherThread_{};
			std::atomic_flag dispatcherThreadRunning_{ATOMIC_FLAG_INIT};

			std::function<void(const Buffer &data)> onPacket_{};
		public:
			HidDeviceThreaded() = default;
			HidDeviceThreaded(const HidDeviceThreaded &other) = delete;
			HidDeviceThreaded& operator=(const HidDeviceThreaded &other) = delete;
			~HidDeviceThreaded();

			bool OpenDevice(const char *path, std::function<void(const Buffer &data)> onPacket);
			void CloseDevice();

			int SendPacket(const Buffer &data);
		};

		struct HidDeviceInfo
		{
			std::string devicePath;
			std::wstring manufactureName;
			std::wstring productName;
			std::wstring serialNumber;
		};

		using HidDeviceList = std::vector<HidDeviceInfo>;

		HidDeviceList EnumerateConnectedDevices(unsigned short vendorId, unsigned short productId);
	}
}
