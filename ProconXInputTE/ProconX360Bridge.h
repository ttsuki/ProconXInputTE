#pragma once
#include "ProControllerHid/ProController.h"
#include "ViGEmClient/ViGEmClientCpp.h"

#include <thread>
#include <mutex>
#include <atomic>

namespace ProconXInputTE
{
	class ProconX360Bridge final
	{
	public:
		using Clock = ProControllerHid::Clock;

		using Timestamp = ProControllerHid::Timestamp;

		struct Options { };

		template <class T>
		struct StatusWithTimeStamp
		{
			Timestamp Timestamp{};
			T Status{};
		};

		struct RumbleParameters
		{
			uint8_t Frequency;
			uint8_t DecaySpeed;
			uint8_t MaxAmplitude;
		};

		struct RumbleParams
		{
			struct
			{
				uint8_t Frequency;
				uint8_t DecaySpeed;
				uint8_t MaxAmplitude;
			} Left, Right;
		};

		struct RumbleStatus
		{
			Timestamp Timestamp;

			struct
			{
				int Left, Right;
			} Large, Small;

			ProControllerHid::ProController::BasicRumble Output;
		};

	private:
		mutable std::mutex mutex_{};

		std::unique_ptr<ProControllerHid::ProController> controller_{};
		std::unique_ptr<ViGEm::X360Controller> x360_{};

		std::atomic<ProControllerHid::InputStatus> last_input_{};
		std::atomic<StatusWithTimeStamp<ViGEm::X360InputStatus>> last_input_sent_{};
		std::atomic<StatusWithTimeStamp<ViGEm::X360OutputStatus>> last_output_{};
		std::atomic<RumbleStatus> last_output_sent_{};

		std::thread rumble_thread_{};
		std::atomic_flag rumble_thread_running_{};
		RumbleParams large_rumble_parameter_ = {{130, 20, 216}, {142, 20, 216}};
		RumbleParams small_rumble_parameter_ = {{72, 30, 176}, {100, 30, 176}};

	public:
		ProconX360Bridge(
			const char* procon_device_path,
			ViGEm::ViGEmClient* client,
			Options options = Options{},
			std::function<void(const char*)> log = nullptr);

		~ProconX360Bridge();

		[[nodiscard]] int GetIndex() const;

		void SetRumbleParameter(RumbleParams large_to_low, RumbleParams small_to_high);

		[[nodiscard]] ProControllerHid::InputStatus GetLastInput() const { return last_input_.load(std::memory_order_acquire); }
		[[nodiscard]] StatusWithTimeStamp<ViGEm::X360InputStatus> GetLastInputSent() const { return last_input_sent_.load(std::memory_order_acquire); }
		[[nodiscard]] StatusWithTimeStamp<ViGEm::X360OutputStatus> GetLastOutputIn() const { return last_output_.load(std::memory_order_acquire); }
		[[nodiscard]] RumbleStatus GetLastOutputOut() const { return last_output_sent_.load(std::memory_order_acquire); }
	};
}
