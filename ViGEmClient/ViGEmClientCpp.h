#pragma once
#include <memory>
#include <functional>

namespace ViGEm
{
	struct X360InputStatus
	{
		struct ButtonStatus
		{
			unsigned Up : 1;
			unsigned Down : 1;
			unsigned Left : 1;
			unsigned Right : 1;

			unsigned Start : 1;
			unsigned Back : 1;
			unsigned LStick : 1;
			unsigned RStick : 1;

			unsigned LButton : 1;
			unsigned RButton : 1;
			unsigned GuideButton : 1;
			unsigned _reserved : 1;
			unsigned AButton : 1;
			unsigned BButton : 1;
			unsigned XButton : 1;
			unsigned YButton : 1;
			unsigned LTrigger : 8;
			unsigned RTrigger : 8;
		} buttons;

		int16_t LeftStickAxisX;
		int16_t LeftStickAxisY;
		int16_t RightStickAxisX;
		int16_t RightStickAxisY;
	};

	struct X360OutputStatus
	{
		uint8_t largeRumble;
		uint8_t smallRumble;
		uint8_t ledNumber;
	};

	class X360Controller
	{
	public:
		X360Controller() = default;
		X360Controller(const X360Controller& other) = delete;
		X360Controller& operator=(const X360Controller& other) = delete;
		virtual ~X360Controller() = default;
		virtual void Report(X360InputStatus inputStatus) = 0;
		virtual int GetDeviceIndex() const = 0;
		virtual void StartNotification() = 0;
		virtual void StopNotification() = 0;
	};

	class ViGEmClient
	{
	public:
		ViGEmClient() = default;
		ViGEmClient(const ViGEmClient& other) = delete;
		ViGEmClient& operator=(const ViGEmClient& other) = delete;
		virtual ~ViGEmClient() = default;

		virtual bool IsConnected() const = 0;
		virtual std::unique_ptr<X360Controller> AddX360Controller(
			std::function<void(const X360OutputStatus& status)> callback) = 0;

		static std::shared_ptr<ViGEmClient> Connect();
	};
}
