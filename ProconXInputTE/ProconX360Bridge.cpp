#include "ProconX360Bridge.h"
#include <algorithm>
#include <iostream>

namespace ProconXInputTE
{
	using ProControllerHid::ProController;
	using ProControllerHid::InputStatus;
	using ViGEm::X360Controller;
	using ViGEm::X360InputStatus;
	using ViGEm::X360OutputStatus;

	ProconX360Bridge::ProconX360Bridge(const char* procon_device_path, ::ViGEm::ViGEmClient* client)
	{
		x360_ = client->AddX360Controller();
		auto index = x360_->GetDeviceIndex();

		controller_ = ProController::Connect(
			procon_device_path,
			false,
			[index](const char* log) { std::cerr << "device[" << std::to_string(index) << "]: " << log << std::endl; }
		);

		if (!controller_)
			throw std::runtime_error("Failed to open ProController.");

		controller_->SetPlayerLed(static_cast<uint8_t>(1u << static_cast<int>(x360_->GetDeviceIndex())));


		x360_->StartNotification([this](const X360OutputStatus& x360Output)
		{
			lastOutput_ = {GetCurrentTimestamp(), x360Output};
		});

		controller_->SetInputStatusCallback([this](const InputStatus& input)
		{
			X360InputStatus status =
			{
				{
					input.Buttons.UpButton,
					input.Buttons.DownButton,
					input.Buttons.LeftButton,
					input.Buttons.RightButton,
					input.Buttons.PlusButton,
					input.Buttons.MinusButton,
					input.Buttons.LStick,
					input.Buttons.RStick,
					input.Buttons.LButton,
					input.Buttons.RButton,
					input.Buttons.HomeButton,
					false,
					input.Buttons.AButton,
					input.Buttons.BButton,
					input.Buttons.XButton,
					input.Buttons.YButton,

					static_cast<uint8_t>(input.Buttons.LZButton ? 255 : 0),
					static_cast<uint8_t>(input.Buttons.RZButton ? 255 : 0),
				},
				static_cast<int16_t>(input.LeftStick.X * 32767),
				static_cast<int16_t>(input.LeftStick.Y * 32767),
				static_cast<int16_t>(input.RightStick.X * 32767),
				static_cast<int16_t>(input.RightStick.Y * 32767),
			};
			x360_->SendReport(status);

			auto timestamp = GetCurrentTimestamp();
			lastInput_ = {timestamp, input};
			lastInputSent_ = {timestamp, status};
		});

		rumble_thread_running_.test_and_set();
		rumble_thread_ = std::thread([this]
		{
			auto clock = std::chrono::steady_clock::now();
			while (rumble_thread_running_.test_and_set())
			{
				large_rumble_value_.first = std::max<int>(large_rumble_value_.first - large_rumble_parameter_.Left.DecaySpeed, 0);
				small_rumble_value_.first = std::max<int>(small_rumble_value_.first - small_rumble_parameter_.Left.DecaySpeed, 0);
				large_rumble_value_.second = std::max<int>(large_rumble_value_.second - large_rumble_parameter_.Right.DecaySpeed, 0);
				small_rumble_value_.second = std::max<int>(small_rumble_value_.second - small_rumble_parameter_.Right.DecaySpeed, 0);
				{
					auto o = lastOutput_.load();
					large_rumble_value_.first = std::max<int>(large_rumble_value_.first, o.second.LargeRumble);
					small_rumble_value_.first = std::max<int>(small_rumble_value_.first, o.second.SmallRumble);
					large_rumble_value_.second = std::max<int>(large_rumble_value_.second, o.second.LargeRumble);
					small_rumble_value_.second = std::max<int>(small_rumble_value_.second, o.second.SmallRumble);
				}

				controller_->SetRumbleBasic(
					static_cast<uint8_t>(large_rumble_value_.first * large_rumble_parameter_.Left.MaxAmplitude / 255),
					static_cast<uint8_t>(large_rumble_value_.second * large_rumble_parameter_.Right.MaxAmplitude / 255),
					static_cast<uint8_t>(small_rumble_value_.first * small_rumble_parameter_.Left.MaxAmplitude / 255),
					static_cast<uint8_t>(small_rumble_value_.second * small_rumble_parameter_.Right.MaxAmplitude / 255),
					large_rumble_parameter_.Left.Frequency,
					large_rumble_parameter_.Right.Frequency,
					small_rumble_parameter_.Left.Frequency,
					small_rumble_parameter_.Right.Frequency
				);

				{
					lastOutputOut_ = {
						GetCurrentTimestamp(), {
							static_cast<uint8_t>(large_rumble_value_.first),
							static_cast<uint8_t>(small_rumble_value_.first),
							0
						}
					};
				}

				clock += std::chrono::milliseconds(16);
				std::this_thread::sleep_until(clock);
			}

			controller_->SetRumbleBasic(0, 0, 0, 0);
		});
	}

	ProconX360Bridge::~ProconX360Bridge()
	{
		controller_->SetInputStatusCallback(nullptr);
		x360_->StopNotification();
		rumble_thread_running_.clear();
		rumble_thread_.join();
		controller_.reset();
		x360_.reset();
	}
}
