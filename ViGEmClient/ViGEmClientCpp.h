// ViGEm Client RAII wrapper

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

	using X360OutputCallback = std::function<void(const X360OutputStatus& status)>;

	class X360Controller
	{
	public:
		X360Controller() = default;
		X360Controller(const X360Controller& other) = delete;
		X360Controller(X360Controller&& other) noexcept = delete;
		X360Controller& operator=(const X360Controller& other) = delete;
		X360Controller& operator=(X360Controller&& other) noexcept = delete;
		virtual ~X360Controller() = default;

		virtual unsigned long GetDeviceIndex() const = 0;

		virtual void SendReport(X360InputStatus inputStatus) = 0;
		virtual void ReceiveReport(X360OutputStatus* outputStatus) = 0;

		virtual void StartNotification(X360OutputCallback callback) = 0;
		virtual void StopNotification() = 0;
	};

	class ViGEmClient
	{
	public:
		ViGEmClient() = default;
		ViGEmClient(const ViGEmClient& other) = delete;
		ViGEmClient(ViGEmClient&& other) noexcept = delete;
		ViGEmClient& operator=(const ViGEmClient& other) = delete;
		ViGEmClient& operator=(ViGEmClient&& other) noexcept = delete;
		virtual ~ViGEmClient() = default;

		virtual std::unique_ptr<X360Controller> AddX360Controller() = 0;
	};

	std::unique_ptr<ViGEmClient> ConnectToViGEm();
}
