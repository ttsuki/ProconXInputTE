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
		struct RumbleParams
		{
			struct Act
			{
				uint8_t Frequency;
				uint8_t DecaySpeed;
				uint8_t MaxAmplitude;
			} Left, Right;
		};


	private:
		mutable std::mutex mutex_{};

		std::unique_ptr<ProControllerHid::ProController> controller_{};
		std::unique_ptr<ViGEm::X360Controller> x360_{};

		std::atomic<std::pair<int64_t, ProControllerHid::InputStatus>> lastInput_{};
		std::atomic<std::pair<int64_t, ViGEm::X360InputStatus>> lastInputSent_{};
		std::atomic<std::pair<int64_t, ViGEm::X360OutputStatus>> lastOutput_{};
		std::atomic<std::pair<int64_t, ViGEm::X360OutputStatus>> lastOutputOut_{};

		std::thread rumble_thread_{};
		std::atomic_flag rumble_thread_running_{};
		RumbleParams large_rumble_parameter_ = { {130, 20, 216}, {142, 20, 216}, };
		RumbleParams small_rumble_parameter_ = { {72, 30, 176}, {100, 30, 176} };
		std::pair<int, int> large_rumble_value_{};
		std::pair<int, int> small_rumble_value_{};

	public:
		static int64_t GetCurrentTimestamp() noexcept
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		}

		ProconX360Bridge(const char* procon_device_path, ::ViGEm::ViGEmClient* client);
		~ProconX360Bridge();

		[[nodiscard]] int GetIndex() const
		{
			return static_cast<int>(x360_->GetDeviceIndex());
		}

		void SetRumbleParameter(RumbleParams large_to_low, RumbleParams small_to_high)
		{
			large_rumble_parameter_ = large_to_low;
			small_rumble_parameter_ = small_to_high;
		}

		[[nodiscard]] std::pair<uint64_t, ProControllerHid::InputStatus> GetLastInput() const { return lastInput_.load(); }
		[[nodiscard]] std::pair<uint64_t, ViGEm::X360InputStatus> GetLastInputSent() const { return lastInputSent_.load(); }
		[[nodiscard]] std::pair<uint64_t, ViGEm::X360OutputStatus> GetLastOutputIn() const { return lastOutput_.load(); }
		[[nodiscard]] std::pair<uint64_t, ViGEm::X360OutputStatus> GetLastOutputOut() const { return lastOutputOut_.load(); }
	};
}
