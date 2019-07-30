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

		Buffer& Buffer::operator +=(const Buffer& data)
		{
			for (int i = 0; i < data.size(); i++)
			{
				buffer_[size_ + i] = data[i];
			}
			size_ += data.size();
			return *this;
		}

		HidDevice::~HidDevice()
		{
			CloseDevice();
		}

		bool HidDevice::OpenDevice(const char* path)
		{
			dev_ = hid_open_path(path);
			return dev_;
		}

		void HidDevice::CloseDevice()
		{
			hid_close(dev_);
			dev_ = nullptr;
		}

		int HidDevice::SendPacket(const Buffer& data)
		{
			return hid_write(dev_, data.data(), data.size());
		}

		Buffer HidDevice::ReceivePacket(int millisec)
		{
			Buffer buffer;
			if (dev_)
			{
				int sz = hid_read_timeout(dev_, buffer.data(), buffer.capacity(), millisec);
				buffer.resize(std::max<int>(sz, 0));
			}
			return buffer;
		}

		HidDeviceThreaded::~HidDeviceThreaded()
		{
			HidDeviceThreaded::CloseDevice();
		}

		bool HidDeviceThreaded::OpenDevice(const char* path)
		{
			if (!device_.OpenDevice(path))
			{
				return false;
			}

			if (!receiverThread_.joinable())
			{
				running_.test_and_set();
				receiverThread_ = std::thread([this, path = std::string(path)]
					{
						SysDep::SetThreadName((std::string(path) + "-ReaderThread").c_str());
						SysDep::SetThreadPriorityToRealtime();
						while (running_.test_and_set())
						{
							Buffer data = device_.ReceivePacket(10);
							if (!data.size()) { continue; }
							OnPacket(data);
							{
								std::lock_guard<decltype(packetCallbackMutex_)> lock(packetCallbackMutex_);
								if (packetCallback_)
								{
									packetCallback_(data);
								}
							}
						}
					}
				);
			}
			return true;
		}

		void HidDeviceThreaded::CloseDevice()
		{
			if (receiverThread_.joinable())
			{
				running_.clear();
				receiverThread_.join();
			}
			packetCallback_ = nullptr;
			device_.CloseDevice();
		}

		int HidDeviceThreaded::SendPacket(const Buffer& data)
		{
			return device_.SendPacket(data);
		}

		void HidDeviceThreaded::ExchangePacket(const Buffer& send, const std::function<bool(const Buffer& received)>& wait)
		{
			std::atomic_flag goNext{ ATOMIC_FLAG_INIT };
			goNext.test_and_set();
			{
				std::lock_guard<decltype(packetCallbackMutex_)> lock(packetCallbackMutex_);
				packetCallback_ = [&](const Buffer& data) { if (wait(data)) { goNext.clear(); } };
			}

			SendPacket(send);
			while (goNext.test_and_set())
			{
				std::this_thread::yield();
			}

			{
				std::lock_guard<decltype(packetCallbackMutex_)> lock(packetCallbackMutex_);
				packetCallback_ = nullptr;
			}
		}
	}
}
