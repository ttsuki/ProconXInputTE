#include "ProconX360Bridge.h"
#include <algorithm>
#include <iostream>

namespace ProconXInputTE
{
	using ProControllerHid::ProController;
	using ProControllerHid::InputStatus;
	using ViGEm::X360InputStatus;
	using ViGEm::X360OutputStatus;

	ProconX360Bridge::ProconX360Bridge(
		const char* procon_device_path,
		ViGEm::ViGEmClient* client,
		Options,
		std::function<void(const char*)> log)
	{
		x360_ = client->AddX360Controller();

		controller_ = ProController::Connect(
			procon_device_path,
			false,
			std::move(log)
		);

		if (!controller_)
			throw std::runtime_error("Failed to open ProController.");

		controller_->SetPlayerLed(static_cast<uint8_t>(1u << static_cast<int>(x360_->GetDeviceIndex())));

		x360_->StartNotification([this](const X360OutputStatus& x360Output)
		{
			last_output_.store({Clock::now(), x360Output}, std::memory_order_release);
		});

		controller_->SetInputStatusCallback([this](const InputStatus& input)
		{
			auto timestamp = Clock::now();

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

			last_input_.store(input, std::memory_order_release);
			last_input_sent_.store({timestamp, status}, std::memory_order_release);
		});

		rumble_thread_ = std::thread([this]
		{
			auto clock = std::chrono::steady_clock::now();
			decltype(RumbleStatus::Large) large{};
			decltype(RumbleStatus::Small) small{};
			rumble_thread_running_.test_and_set();

			while (rumble_thread_running_.test_and_set())
			{
				const auto latest = last_output_.load(std::memory_order_acquire).Status;

				large = {
					std::max<int>(large.Left - large_rumble_parameter_.Left.DecaySpeed, latest.LargeRumble),
					std::max<int>(large.Right - large_rumble_parameter_.Right.DecaySpeed, latest.LargeRumble)
				};

				small = {
					std::max<int>(small.Left - small_rumble_parameter_.Left.DecaySpeed, latest.SmallRumble),
					std::max<int>(small.Right - small_rumble_parameter_.Right.DecaySpeed, latest.SmallRumble)
				};

				const ProController::BasicRumble output = {
					{
						{small_rumble_parameter_.Left.Frequency, static_cast<uint8_t>(small.Left * small_rumble_parameter_.Left.MaxAmplitude / 255)},
						{large_rumble_parameter_.Left.Frequency, static_cast<uint8_t>(large.Left * large_rumble_parameter_.Left.MaxAmplitude / 255)}
					},
					{
						{small_rumble_parameter_.Right.Frequency, static_cast<uint8_t>(small.Right * small_rumble_parameter_.Right.MaxAmplitude / 255)},
						{large_rumble_parameter_.Right.Frequency, static_cast<uint8_t>(large.Right * large_rumble_parameter_.Right.MaxAmplitude / 255)}
					}
				};

				controller_->SetRumble(output);
				last_output_sent_.store({Clock::now(), large, small, output}, std::memory_order_release);

				std::this_thread::sleep_until(clock += std::chrono::milliseconds(16));
			}

			controller_->SetRumble(ProController::BasicRumble{});
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
		std::this_thread::sleep_for(std::chrono::milliseconds(10)); // waits for exit of x360 thread
	}

	int ProconX360Bridge::GetIndex() const
	{
		return static_cast<int>(x360_->GetDeviceIndex());
	}

	void ProconX360Bridge::SetRumbleParameter(RumbleParams large_to_low, RumbleParams small_to_high)
	{
		large_rumble_parameter_ = large_to_low;
		small_rumble_parameter_ = small_to_high;
	}
}
