#include "HidDevice.h"
#include "SysDep.h"

#include <string>

namespace ProControllerHid
{
	namespace HidIo
	{
		Buffer::Buffer(std::initializer_list<uint8_t> list)
		{
			std::copy(list.begin(), list.end(), buffer_.begin());
			size_ = list.size();
		}

		Buffer& Buffer::operator +=(const Buffer &data)
		{
			for (int i = 0; i < data.size(); i++)
			{
				buffer_[size_ + i] = data[i];
			}
			size_ += data.size();
			return *this;
		}

		void BufferQueue::Signal(const Buffer &data)
		{
			std::lock_guard<decltype(mutex_)> lock(mutex_);
			if (queue_.size() < 16)
			{
				queue_.push(data);
				signal_.notify_one();
			}
		}

		Buffer BufferQueue::Wait()
		{
			std::unique_lock<decltype(mutex_)> lock(mutex_);
			signal_.wait(lock, [this] { return !queue_.empty(); });
			Buffer data = queue_.front();
			queue_.pop();
			return data;
		}

		void BufferQueue::Clear()
		{
			std::lock_guard<decltype(mutex_)> lock(mutex_);
			while (!queue_.empty()) { queue_.pop(); }
		}

		HidDevice::~HidDevice()
		{
			CloseDevice();
		}

		bool HidDevice::OpenDevice(const char *path)
		{
			dev_ = SysDep::HidDevice::OpenHidDevice(path);
			return dev_;
		}

		void HidDevice::CloseDevice()
		{
			if (dev_)
			{
				static_cast<SysDep::HidDevice*>(dev_)->Close();
				dev_ = nullptr;
			}
			dev_ = nullptr;
		}

		int HidDevice::SendPacket(const Buffer &data)
		{
			if (dev_)
			{
				return static_cast<SysDep::HidDevice*>(dev_)->SendPacket(data.data(), data.size());
			}
			return 0;
		}

		Buffer HidDevice::ReceivePacket(int millisec)
		{
			Buffer buffer;
			if (dev_)
			{
				int sz = static_cast<SysDep::HidDevice*>(dev_)->ReceivePacket(buffer.data(), buffer.capacity(), millisec);
				buffer.resize(std::max<int>(sz, 0));
			}
			return buffer;
		}

		HidDeviceThreaded::~HidDeviceThreaded()
		{
			CloseDevice();
		}

		bool HidDeviceThreaded::OpenDevice(const char *path, std::function<void(const Buffer &data)> onPacket)
		{
			CloseDevice();

			if (!deviceToSend_.OpenDevice(path))
			{
				return false;
			}

			if (!deviceToReceive_.OpenDevice(path))
			{
				deviceToSend_.CloseDevice();
				return false;
			}

			onPacket_ = std::move(onPacket);

			senderQueue_.Clear();
			receiveQueue_.Clear();

			senderThreadRunning_.test_and_set();
			senderThread_ = std::thread([this, threadName = std::string(path) + "-SenderThread"]
			{
				SysDep::SetThreadName(threadName.c_str());
				SysDep::SetThreadPriorityToRealtime();
				while (true)
				{
					Buffer data = senderQueue_.Wait();
					if (!senderThreadRunning_.test_and_set()) { break; }
					if (data.size())
					{
						if (deviceToSend_.SendPacket(data) < 0)
						{
							// failed.
						}
					}
				}
			});

			receiverThreadRunning_.test_and_set();
			receiverThread_ = std::thread([this, threadName = std::string(path) + "-ReceiverThread"]
			{
				SysDep::SetThreadName(threadName.c_str());
				SysDep::SetThreadPriorityToRealtime();
				while (receiverThreadRunning_.test_and_set())
				{
					Buffer data;
					{
						while (true)
						{
							data = deviceToReceive_.ReceivePacket(100);
							if (data.size()) { break; }
						}
					}
					receiveQueue_.Signal(data);
				}
			});

			dispatcherThreadRunning_.test_and_set();
			dispatcherThread_ = std::thread([this, threadName = std::string(path) + "-DispatcherThread"]
			{
				SysDep::SetThreadName(threadName.c_str());
				SysDep::SetThreadPriorityToRealtime();
				while (true)
				{
					Buffer data = receiveQueue_.Wait();
					if (!dispatcherThreadRunning_.test_and_set()) { break; }
					onPacket_(data);
				}
			});

			return running_ = true;
		}

		void HidDeviceThreaded::CloseDevice()
		{
			running_ = false;

			senderThreadRunning_.clear();
			if (senderThread_.joinable())
			{
				senderQueue_.Signal({});
				senderThread_.join();
			}

			receiverThreadRunning_.clear();
			if (receiverThread_.joinable())
			{
				receiverThread_.join();
			}

			dispatcherThreadRunning_.clear();
			if (dispatcherThread_.joinable())
			{
				receiveQueue_.Signal({});
				dispatcherThread_.join();
			}

			deviceToReceive_.CloseDevice();
			deviceToSend_.CloseDevice();

			onPacket_ = nullptr;
		}

		int HidDeviceThreaded::SendPacket(const Buffer &data)
		{
			if (!running_) { return 0; }

			senderQueue_.Signal(data);
			return data.size();
		}

		HidDeviceList EnumerateConnectedDevices(unsigned short vendorId, unsigned short productId)
		{
			const auto devices = SysDep::EnumerateAllConnectedHidDevicePaths(vendorId, productId);
			HidDeviceList result;
			for (auto&& dev : devices)
			{
				HidDeviceInfo device{};
				device.devicePath = dev.devicePath;
				device.manufactureName = dev.manufactureName;
				device.productName = dev.productName;
				device.serialNumber = dev.serialNumber;
				result.push_back(device);
			}
			return result;
		}
	}
}
