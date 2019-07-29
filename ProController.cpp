#include "ProController.h"

#include <algorithm>
#include <thread>
#include <chrono>

#include <hidapi.h>

#include "HidDevice.h"

namespace ProControllerHid
{
	using Buffer = HidIo::Buffer;

	static uint64_t tick()
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

	static void DumpPacket(const char* msg, const Buffer& data, int start = 0, int count = -1, uint64_t clock = tick())
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

	class ProControllerImpl final : HidIo::HidDeviceThreaded, public ProController
	{
		uint8_t nextPacketNumber_{};
		HidIo::Buffer rumbleStatus_{};
		uint8_t playerLedStatus_{};

		std::thread controllerUpdaterThread_{};
		std::atomic_flag running_{ ATOMIC_FLAG_INIT };

		std::function<void(const InputStatus& status)> statusCallback_{};
		bool statusCallbackEnabled_{};

	public:
		ProControllerImpl(const hid_device_info* devInfo, int index,
			std::function<void(const InputStatus& status)> statusCallback);
		~ProControllerImpl() override;
		
		void StartStatusCallback() override;
		void StopStatusCallback() override;

		void SetRumbleBasic(
			uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp, uint8_t rightHighAmp,
			uint8_t leftLowFreq, uint8_t rightLowFreq, uint8_t leftHighFreq, uint8_t rightHighFreq) override;

		void SetPlayerLed(uint8_t playerLed) override;

	private:
		void SendUsbCommand(uint8_t usbCommand, const HidIo::Buffer& data, bool waitAck);
		void SendSubCommand(uint8_t subCommand, const HidIo::Buffer& data, bool waitAck);
		void SendRumble();

		void OnPacket(const HidIo::Buffer& data) override;
		void OnStatus(const HidIo::Buffer& data);
	};

	ProControllerImpl::ProControllerImpl(const hid_device_info* devInfo, int index,
		std::function<void(const InputStatus& status)> statusCallback)
	{
		rumbleStatus_ = { 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };

		if (!OpenDevice(devInfo->path))
		{
			return;
		}

		SendUsbCommand(0x02, {}, true); // Handshake
		SendUsbCommand(0x03, {}, true); // Set baudrate to 3Mbps
		SendUsbCommand(0x02, {}, true); // Handshake
		SendUsbCommand(0x04, {}, false); // HID only-mode(No use Bluetooth)

		SendSubCommand(0x40, { 0x00 }, true); // disable imuData
		SendSubCommand(0x48, { 0x01 }, true); // enable Rumble
		SendSubCommand(0x38, { 0x2F, 0x10, 0x11, 0x33, 0x33 }, true); // Set HOME Light
		SendSubCommand(0x30, { playerLedStatus_ = 0x3 | index << 2 }, true);

		statusCallback_ = std::move(statusCallback);
		running_.test_and_set();
		controllerUpdaterThread_ = std::thread([this]
			{
				decltype(playerLedStatus_) playerLedStatus = 0;

				while (running_.test_and_set())
				{
					if (playerLedStatus != playerLedStatus_)
					{
						playerLedStatus = playerLedStatus_;
						SendSubCommand(0x30, { playerLedStatus_ }, false);
					}
					else
					{
						SendRumble();
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(16));
				}
			}
		);
	}

	ProControllerImpl::~ProControllerImpl()
	{
		if (controllerUpdaterThread_.joinable())
		{
			running_.clear();
			controllerUpdaterThread_.join();
		}

		SendSubCommand(0x38, { 0x00 }, true); // Set HOME Light
		SendSubCommand(0x30, { 0x00 }, true); // Set Player LED
		//ExecSubCommand(0x06, { 0x00 }, true); // Set HCI state (sleep mode)
		SendUsbCommand(0x05, {}, false); // Allows the Joy-Con or Pro Controller to time out and talk Bluetooth again.
	}

	void ProControllerImpl::StartStatusCallback()
	{
		statusCallbackEnabled_ = true;
	}

	void ProControllerImpl::StopStatusCallback()
	{
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

	void ProControllerImpl::SendUsbCommand(uint8_t usbCommand, const Buffer& data, bool waitAck)
	{
		Buffer buf = {
			0x80,
		};
		buf += {usbCommand};
		buf += data;
		DumpPacket("UsbCmd>", buf);

		if (waitAck)
		{
			ExchangePacket(buf, [&](const Buffer& r)
				{
					if (r[0] == 0x81 && r[1] == usbCommand)
					{
						DumpPacket("UsbAck<", data, 0, 2);
						return true;
					}
					return false;
				});
		}
		else
		{
			SendPacket(buf);
		}
	}

	void ProControllerImpl::SendSubCommand(uint8_t subCommand, const Buffer& data, bool waitAck)
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
		if (waitAck)
		{
			ExchangePacket(buf, [&](const Buffer& r)
				{
					if (r[0] == 0x21 && r[14] == subCommand)
					{
						DumpPacket("SubAck<", data, 14, 8);
						return true;
					}
					return false;
				});
		}
		else
		{
			SendPacket(buf);
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
		SendPacket(buf);
	}

	void ProControllerImpl::OnPacket(const Buffer& data)
	{
		if (data.size())
		{
			switch (data[0])
			{
			case 0x21: // Reply to sub command.
			case 0x30: // Input status full
			case 0x31: // Input status full
				OnStatus(data);
				break;
			default:
				DumpPacket("Packet<", data, 0, 16);
				break;
			}
		}
	}

	void ProControllerImpl::OnStatus(const Buffer& data)
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

		if (statusCallbackEnabled_ && statusCallback_)
		{
			InputStatus status = {};
			status.clock = tick();
			status.LeftStick = lStick.status;
			status.RightStick = rStick.status;
			status.Buttons = buttons.status;
			statusCallback_(status);
		}
	}

	std::unique_ptr<ProController> ProController::Connect(const hid_device_info* devInfo, int index,
		std::function<void(const InputStatus& status)> statusCallback)
	{
		return std::make_unique<ProControllerImpl>(devInfo, index, statusCallback);
	}

	HidDeviceCollection EnumerateProControllers()
	{
		constexpr unsigned short kNintendoVID{ 0x057E };
		constexpr unsigned short kProControllerPID{ 0x2009 };
		return HidDeviceCollection::EnumerateDevices(kNintendoVID, kProControllerPID);
	}
}
