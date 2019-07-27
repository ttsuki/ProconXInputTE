#pragma once

#include <hidapi.h>

#include "HidDevice.h"

namespace ProconXInputTE
{
	class ControllerDevice
	{
	public:
		ControllerDevice() = default;
		ControllerDevice(const ControllerDevice& other) = delete;
		ControllerDevice& operator=(const ControllerDevice& other) = delete;
		virtual ~ControllerDevice() = default;
	};

	struct StickStatus
	{
		unsigned AxisX : 12;
		unsigned AxisY : 12;
		unsigned padding : 8;
	};

	struct ButtonStatus
	{
		unsigned YButton : 1;
		unsigned XButton : 1;
		unsigned BButton : 1;
		unsigned AButton : 1;
		unsigned RSRButton : 1;
		unsigned RSLButton : 1;
		unsigned RButton : 1;
		unsigned RZButton : 1;

		unsigned MinusButton : 1;
		unsigned PlusButton : 1;
		unsigned RStick : 1;
		unsigned LStick : 1;
		unsigned HomeButton : 1;
		unsigned ShareButton : 1;
		unsigned Unknown3 : 1;
		unsigned ChargingGrip : 1;

		unsigned DownButton : 1;
		unsigned UpButton : 1;
		unsigned RightButton : 1;
		unsigned LeftButton : 1;
		unsigned LSRButton : 1;
		unsigned LSLButton : 1;
		unsigned LButton : 1;
		unsigned LZButton : 1;

		unsigned padding_ : 8;
	};

	struct InputStatus
	{
		uint64_t clock;
		StickStatus LeftStick;
		StickStatus RightStick;
		ButtonStatus Buttons;
	};

	class ProController final : HidDeviceThreaded, public ControllerDevice
	{
		uint8_t nextPacketNumber_{};
		Buffer rumbleStatus_{};
		uint8_t playerLedStatus_{};

		std::thread controllerUpdaterThread_{};
		std::atomic_flag running_{ ATOMIC_FLAG_INIT };

		std::function<void(const InputStatus& status)> statusCallback_{};
	public:
		ProController(const hid_device_info* devInfo, int index,
			std::function<void(const InputStatus& status)> statusCallback);
		~ProController() override;

		void SetRumbleBasic(
			uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp = 0x00, uint8_t rightHighAmp = 0x00,
			uint8_t leftLowFreq = 0x80, uint8_t rightLowFreq = 0x80, uint8_t leftHighFreq = 0x80, uint8_t rightHighFreq = 0x80);
		void SetPlayerLed(uint8_t playerLed);

	private:
		void SendUsbCommand(uint8_t usbCommand, const Buffer& data, bool waitAck);
		void SendSubCommand(uint8_t subCommand, const Buffer& data, bool waitAck);
		void SendRumble();

		void OnPacket(const Buffer& data) override;
		void OnStatus(const Buffer& data);
	};
}
