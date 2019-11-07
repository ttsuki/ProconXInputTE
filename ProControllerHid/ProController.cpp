#include "ProController.h"

#include <algorithm>
#include <thread>
#include <chrono>
#include <shared_mutex>
#include <map>

#include "HidDevice.h"
#include "SysDep.h"

namespace ProControllerHid
{
	using Buffer = HidIo::Buffer;

	static uint64_t tick()
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

	static void DumpPacket(const char *msg, const Buffer &data, int start = 0, int count = -1, uint64_t clock = tick())
	{
#ifdef LOG_PACKET
		static std::mutex m;
		std::lock_guard<decltype(m)> lock(m);
		if (count == -1) { count = data.size() - start; }
		printf("\x1b[2K%lld: %9s", clock, msg);
		for (int i = start; i < start + count; i++) { printf(" %02X", data[i]); }
		printf("\n");
#endif
	}

	class ProControllerImpl final : public ProController
	{
		HidIo::HidDeviceThreaded device_{};

		uint8_t nextPacketNumber_{};
		Buffer rumbleStatus_{};
		uint8_t playerLedStatus_{};

		std::thread controllerUpdaterThread_{};
		std::atomic_flag controllerUpdaterThreadRunning_{ATOMIC_FLAG_INIT};

		std::function<void(const InputStatus &status)> statusCallback_{};
		bool statusCallbackEnabled_{};
		std::mutex statusCallbackCalling_{};

		class PendingCommandMap
		{
			std::shared_mutex mutex_{};
			std::condition_variable_any signal_{};
			std::map<uint8_t, std::chrono::steady_clock::time_point> pending_{};
		public:
			void Register(uint8_t command, std::chrono::milliseconds timeout = std::chrono::milliseconds(60));
			void Signal(uint8_t command);
			bool Pending(uint8_t command);
			bool Wait(uint8_t command);
		} usbCommandQueue_{}, subCommandQueue_{};

	public:
		ProControllerImpl(const char *pathToDevice, int index,
			std::function<void(const InputStatus &status)> statusCallback);
		~ProControllerImpl() override;

		void StartStatusCallback() override;
		void StopStatusCallback() override;

		void SetRumbleBasic(
			uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp, uint8_t rightHighAmp,
			uint8_t leftLowFreq, uint8_t rightLowFreq, uint8_t leftHighFreq, uint8_t rightHighFreq) override;

		void SetPlayerLed(uint8_t playerLed) override;

	private:
		void SendUsbCommand(uint8_t usbCommand, const Buffer &data, bool waitAck);
		void SendSubCommand(uint8_t subCommand, const Buffer &data, bool waitAck);
		void SendRumble();

		void OnPacket(const Buffer &data);
		void OnStatus(const Buffer &data);
	};

	void ProControllerImpl::PendingCommandMap::Register(uint8_t command, std::chrono::milliseconds timeout)
	{
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		pending_[command] = std::chrono::steady_clock::now() + timeout;
	}

	void ProControllerImpl::PendingCommandMap::Signal(uint8_t command)
	{
		std::lock_guard<decltype(mutex_)> lock(mutex_);
		pending_.erase(command);
		signal_.notify_all();
	}

	bool ProControllerImpl::PendingCommandMap::Pending(uint8_t command)
	{
		std::shared_lock<decltype(mutex_)> lock(mutex_);
		auto it = pending_.find(command);
		return it != pending_.end() && it->second < std::chrono::steady_clock::now();
	}

	bool ProControllerImpl::PendingCommandMap::Wait(uint8_t command)
	{
		std::shared_lock<decltype(mutex_)> lock(mutex_);
		auto it = pending_.find(command);
		if (it == pending_.end())
		{
			return true;
		}

		return signal_.wait_until(lock, it->second,
				[this, command] { return pending_.count(command) == 0; })
			|| pending_.count(command) == 0;
	}

	ProControllerImpl::ProControllerImpl(
		const char *pathToDevice, int index,
		std::function<void(const InputStatus &status)> statusCallback)
	{
		SetRumbleBasic(0, 0, 0, 0, 0x80, 0x80, 0x80, 0x80);
		SetPlayerLed((1 << index) - 1);

		bool openResult = device_.OpenDevice(pathToDevice, [this](const Buffer &data) { OnPacket(data); });
		if (!openResult)
		{
			return;
		}

		SendUsbCommand(0x02, {}, true); // Handshake
		SendUsbCommand(0x03, {}, true); // Set baudrate to 3Mbps
		SendUsbCommand(0x02, {}, true); // Handshake
		SendUsbCommand(0x04, {}, false); // HID only-mode(No use Bluetooth)

		SendSubCommand(0x40, {0x00}, true); // disable imuData
		SendSubCommand(0x48, {0x01}, true); // enable Rumble
		SendSubCommand(0x38, {0x2F, 0x10, 0x11, 0x33, 0x33}, true); // Set HOME Light animation
		SendSubCommand(0x30, {playerLedStatus_}, true); // Set Player LED Status

		statusCallback_ = std::move(statusCallback);

		controllerUpdaterThreadRunning_.test_and_set();
		controllerUpdaterThread_ = std::thread([this, threadName = std::string(pathToDevice) + "-UpdaterThread"]
			{
				SysDep::SetThreadName(threadName.c_str());
				SysDep::SetThreadPriorityToRealtime();
				auto clock = std::chrono::steady_clock::now();

				decltype(playerLedStatus_) playerLedStatus = playerLedStatus_;
				decltype(playerLedStatus_) playerLedStatusSending = playerLedStatus_;
				auto updatePlayerLedStatus = [&]
				{
					if (subCommandQueue_.Pending(0x30)) { return false; }
					if (subCommandQueue_.Wait(0x30)) { playerLedStatus = playerLedStatusSending; }
					if (playerLedStatusSending != playerLedStatus_ || playerLedStatus != playerLedStatus_)
					{
						playerLedStatusSending = playerLedStatus_;
						SendSubCommand(0x30, {playerLedStatusSending}, false);
						return true;
					}
					return false;
				};

				while (controllerUpdaterThreadRunning_.test_and_set())
				{
					if (updatePlayerLedStatus())
					{
					}
					else
					{
						SendRumble();
					}
					clock += std::chrono::nanoseconds(16666667);
					std::this_thread::sleep_until(clock);
				}
			}
		);
	}

	ProControllerImpl::~ProControllerImpl()
	{
		if (controllerUpdaterThread_.joinable())
		{
			controllerUpdaterThreadRunning_.clear();
			controllerUpdaterThread_.join();
		}
		SendSubCommand(0x38, {0x00}, true); // Set HOME Light animation
		SendSubCommand(0x30, {0x00}, true); // Set Player LED
		SendUsbCommand(0x05, {}, false); // Allows the Joy-Con or Pro Controller to time out and talk Bluetooth again.
		//ExecSubCommand(0x06, { 0x00 }, false); // Set HCI state (sleep mode)
		device_.CloseDevice();
	}

	void ProControllerImpl::StartStatusCallback()
	{
		std::lock_guard<decltype(statusCallbackCalling_)> lock(statusCallbackCalling_);
		statusCallbackEnabled_ = true;
	}

	void ProControllerImpl::StopStatusCallback()
	{
		std::lock_guard<decltype(statusCallbackCalling_)> lock(statusCallbackCalling_);
		statusCallbackEnabled_ = false;
	}

	void ProControllerImpl::SetRumbleBasic(
		uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp, uint8_t rightHighAmp,
		uint8_t leftLowFreq, uint8_t rightLowFreq, uint8_t leftHighFreq, uint8_t rightHighFreq)
	{
		uint32_t l = 0x40000000u | leftHighFreq >> 1 << 2 | leftHighAmp >> 1 << 9 | leftLowFreq >> 1 << 16 | leftLowAmp >> 1 << 23;
		uint32_t r = 0x40000000u | rightHighFreq >> 1 << 2 | rightHighAmp >> 1 << 9 | rightLowFreq >> 1 << 16 | rightLowAmp >> 1 << 23;

		rumbleStatus_[0] = l, rumbleStatus_[1] = l >> 8, rumbleStatus_[2] = l >> 16, rumbleStatus_[3] = l >> 24;
		rumbleStatus_[4] = r, rumbleStatus_[5] = r >> 8, rumbleStatus_[6] = r >> 16, rumbleStatus_[7] = r >> 24;
	}

	void ProControllerImpl::SetPlayerLed(uint8_t playerLed)
	{
		playerLedStatus_ = playerLed;
	}

	void ProControllerImpl::SendUsbCommand(uint8_t usbCommand, const Buffer &data, bool waitAck)
	{
		Buffer buf = {
			0x80,
		};
		buf += {usbCommand};
		buf += data;
		DumpPacket("UsbCmd>", buf);

		usbCommandQueue_.Register(usbCommand);
		device_.SendPacket(buf);
		if (waitAck)
		{
			while (!usbCommandQueue_.Wait(usbCommand))
			{
				usbCommandQueue_.Register(usbCommand);
				device_.SendPacket(buf);
			}
		}
	}

	void ProControllerImpl::SendSubCommand(uint8_t subCommand, const Buffer &data, bool waitAck)
	{
		Buffer buf = {
			0x01, // SubCommand
			static_cast<uint8_t>(nextPacketNumber_++ & 0xf),
			rumbleStatus_[0], rumbleStatus_[1], rumbleStatus_[2], rumbleStatus_[3],
			rumbleStatus_[4], rumbleStatus_[5], rumbleStatus_[6], rumbleStatus_[7],
		};
		buf += {subCommand};
		buf += data;
		DumpPacket("SubCmd>", buf, 10);

		subCommandQueue_.Register(subCommand);
		device_.SendPacket(buf);

		if (waitAck)
		{
			while (!subCommandQueue_.Wait(subCommand))
			{
				subCommandQueue_.Register(subCommand);
				device_.SendPacket(buf);
			}
		}
	}

	void ProControllerImpl::SendRumble()
	{
		Buffer buf = {
			0x10,
			static_cast<uint8_t>(nextPacketNumber_++ & 0xf),
			rumbleStatus_[0], rumbleStatus_[1], rumbleStatus_[2], rumbleStatus_[3],
			rumbleStatus_[4], rumbleStatus_[5], rumbleStatus_[6], rumbleStatus_[7],
		};
		device_.SendPacket(buf);
	}

	void ProControllerImpl::OnPacket(const Buffer &data)
	{
		if (data.size())
		{
			switch (data[0])
			{
			case 0x30: // Input status full
			case 0x31: // Input status full
				OnStatus(data);
				break;

			case 0x21: // Reply to sub command.
				subCommandQueue_.Signal(data[14]);
				OnStatus(data);
				break;

			case 0x81: // Reply to usb command.
				usbCommandQueue_.Signal(data[1]);
				break;

			default:
				DumpPacket("Packet<", data, 0, 16);
				break;
			}
		}
	}

	void ProControllerImpl::OnStatus(const Buffer &data)
	{
		union
		{
			StickStatus status;
			uint32_t raw;
		} lStick{}, rStick{};

		union
		{
			ButtonStatus status;
			uint32_t raw;
		} buttons{};

		buttons.raw = data[3] | data[4] << 8 | data[5] << 16;
		lStick.raw = data[6] | data[7] << 8 | data[8] << 16;
		rStick.raw = data[9] | data[10] << 8 | data[11] << 16;

		InputStatus status = {};
		status.clock = tick();
		status.LeftStick = lStick.status;
		status.RightStick = rStick.status;
		status.Buttons = buttons.status;

		{
			std::lock_guard<decltype(statusCallbackCalling_)> lock(statusCallbackCalling_);
			if (statusCallbackEnabled_ && statusCallback_)
			{
				statusCallback_(status);
			}
		}
	}

	std::unique_ptr<ProController> ProController::Connect(
		const char *pathToDevice, int index,
		std::function<void(const InputStatus &status)> statusCallback)
	{
		return std::make_unique<ProControllerImpl>(pathToDevice, index, statusCallback);
	}

	std::vector<std::string> ProController::EnumerateProControllerDevicePaths()
	{
		std::vector<std::string> result;
		constexpr unsigned short kNintendoVID{0x057E};
		constexpr unsigned short kProControllerPID{0x2009};
		for (auto &&d : HidIo::EnumerateConnectedDevices(kNintendoVID, kProControllerPID))
		{
			result.push_back(d.devicePath);
		}
		return result;
	}
}
