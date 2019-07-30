
#include "ProconX360Bridge.h"
#include <algorithm>
#include <hidapi.h>
#include "ProControllerHid/SysDep.h"

namespace ProconXInputTE
{
	using ProControllerHid::ProController;
	using ProControllerHid::InputStatus;
	using ViGEm::X360Controller;
	using ViGEm::X360InputStatus;
	using ViGEm::X360OutputStatus;

	ProconX360Bridge::ProconX360Bridge(hid_device_info* proCon, ::ViGEm::ViGEmClient* client)
	{
		x360_ = client->AddX360Controller(
			[this](const X360OutputStatus& x360Output)
			{
				HandleControllerOutput(x360Output);
			});

		controller_ = ProController::Connect(proCon, x360_->GetDeviceIndex(),
			[this](const InputStatus& proconInput)
			{
				HandleControllerInput(proconInput);
			});

		rumbleControlThreadRunning_.test_and_set();
		rumbleControlThread_ = std::thread([this, path = std::string(proCon->path)]
		{
			ProControllerHid::SysDep::SetThreadName((std::string(path) + "-BridgeThread").c_str());
			RumbleControlTreadBody();
		});

		x360_->StartNotification();
		controller_->StartStatusCallback();
	}

	ProconX360Bridge::~ProconX360Bridge()
	{
		controller_->StopStatusCallback();
		x360_->StopNotification();
		rumbleControlThreadRunning_.clear();
		rumbleControlThread_.join();
		controller_.reset();
		x360_.reset();
	}

	void ProconX360Bridge::SetRumbleParameter(RumbleParams largeToLow, RumbleParams smallToHigh)
	{
		largeRumbleParam = largeToLow;
		smallRumbleParam = smallToHigh;
	}

	std::pair<uint64_t, ProControllerHid::InputStatus> ProconX360Bridge::GetLastInput() const
	{
		std::lock_guard<decltype(lastInputMutex_)> lock(lastInputMutex_);
		return lastInput_;
	}

	std::pair<uint64_t, ViGEm::X360OutputStatus> ProconX360Bridge::GetLastOutputIn() const
	{
		std::lock_guard<decltype(lastOutputMutex_)> lock(lastOutputMutex_);
		return lastOutput_;
	}

	std::pair<uint64_t, ViGEm::X360OutputStatus> ProconX360Bridge::GetLastOutputOut() const
	{
		std::lock_guard<decltype(lastOutputOutMutex_)> lock(lastOutputOutMutex_);
		return lastOutputOut_;
	}

	void ProconX360Bridge::HandleControllerOutput(const X360OutputStatus& x360Output)
	{
		largeMoterAmplification_ = std::max<int>(largeMoterAmplification_, x360Output.largeRumble);
		smallMoterAmplification_ = std::max<int>(smallMoterAmplification_, x360Output.smallRumble);
		{
			std::lock_guard<decltype(lastOutputMutex_)> lock(lastOutputMutex_);
			lastOutput_ = { GetCurrentTimestamp(), x360Output };
		}
	}

	void ProconX360Bridge::HandleControllerInput(const InputStatus& inputStatus)
	{
		ViGEm::X360InputStatus status =
		{
			{
				inputStatus.Buttons.UpButton,
				inputStatus.Buttons.DownButton,
				inputStatus.Buttons.LeftButton,
				inputStatus.Buttons.RightButton,
				inputStatus.Buttons.PlusButton,
				inputStatus.Buttons.MinusButton,
				inputStatus.Buttons.LStick,
				inputStatus.Buttons.RStick,
				inputStatus.Buttons.LButton,
				inputStatus.Buttons.RButton,
				inputStatus.Buttons.HomeButton,
				false,
				inputStatus.Buttons.AButton,
				inputStatus.Buttons.BButton,
				inputStatus.Buttons.XButton,
				inputStatus.Buttons.YButton,

				static_cast<uint8_t>(inputStatus.Buttons.LZButton ? 255 : 0),
				static_cast<uint8_t>(inputStatus.Buttons.RZButton ? 255 : 0),
			},

			(static_cast<int16_t>(inputStatus.LeftStick.AxisX) << 4) - 32767,
			(static_cast<int16_t>(inputStatus.LeftStick.AxisY) << 4) - 32767,
			(static_cast<int16_t>(inputStatus.RightStick.AxisX) << 4) - 32767,
			(static_cast<int16_t>(inputStatus.RightStick.AxisY) << 4) - 32767,
		};
		x360_->Report(status);
		
		{
			std::lock_guard<decltype(lastInputMutex_)> lock(lastInputMutex_);
			lastInput_ = { GetCurrentTimestamp(), inputStatus };
		}
	}

	void ProconX360Bridge::RumbleControlTreadBody()
	{
		auto clock = std::chrono::steady_clock::now();
		while (rumbleControlThreadRunning_.test_and_set())
		{
			largeMoterAmplification_ = std::max<int>(largeMoterAmplification_ - largeRumbleParam.DecaySpeed, 0);
			smallMoterAmplification_ = std::max<int>(smallMoterAmplification_ - largeRumbleParam.DecaySpeed, 0);

			uint8_t large = largeMoterAmplification_ * largeRumbleParam.MaxAmplitude / 255;
			uint8_t small = smallMoterAmplification_ * smallRumbleParam.MaxAmplitude / 255;
			controller_->SetRumbleBasic(
				large, large, small, small,
				largeRumbleParam.Frequency,
				largeRumbleParam.Frequency, 
				smallRumbleParam.Frequency, 
				smallRumbleParam.Frequency
			);

			{
				std::lock_guard<decltype(lastOutputOutMutex_)> lock(lastOutputOutMutex_);
				lastOutputOut_ = { GetCurrentTimestamp(), {large, small, 0} };
			}

			clock += std::chrono::milliseconds(16);
			std::this_thread::sleep_until(clock);
		}

		controller_->SetRumbleBasic(0, 0, 0, 0);
	}
	int64_t ProconX360Bridge::GetCurrentTimestamp()
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

}
