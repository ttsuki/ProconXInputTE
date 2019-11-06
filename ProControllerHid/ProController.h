#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace ProControllerHid
{
	class ControllerDevice
	{
	public:
		ControllerDevice() = default;
		ControllerDevice(const ControllerDevice &other) = delete;
		ControllerDevice& operator=(const ControllerDevice &other) = delete;
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

	class ProController : public ControllerDevice
	{
	public:
		ProController() = default;
		ProController(const ProController &other) = delete;
		ProController& operator=(const ProController &other) = delete;
		virtual ~ProController() = default;

		virtual void StartStatusCallback() = 0;
		virtual void StopStatusCallback() = 0;

		virtual void SetRumbleBasic(
			uint8_t leftLowAmp, uint8_t rightLowAmp, uint8_t leftHighAmp = 0x00, uint8_t rightHighAmp = 0x00,
			uint8_t leftLowFreq = 0x80, uint8_t rightLowFreq = 0x80, uint8_t leftHighFreq = 0x80, uint8_t rightHighFreq = 0x80) = 0;

		virtual void SetPlayerLed(uint8_t playerLed) = 0;

		static std::unique_ptr<ProController> Connect(
			const char *pathToDevice, int index,
			std::function<void(const InputStatus &status)> statusCallback);

		static std::vector<std::string> EnumerateProControllerDevicePaths();
	};
}
