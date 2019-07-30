#pragma once
#include "ProControllerHid/ProController.h"
#include "ViGEmClient/ViGEmClientCpp.h"

#include <thread>
#include <mutex>
#include <atomic>

namespace ProconXInputTE
{
	class ProconX360Bridge
	{
	public:

		ProconX360Bridge(hid_device_info* proCon, ::ViGEm::ViGEmClient* client);
		~ProconX360Bridge();

		struct RumbleParams
		{
			uint8_t Frequency;
			uint8_t DecaySpeed;
			uint8_t MaxAmplitude;
		};

		void SetRumbleParameter(RumbleParams largeToLow, RumbleParams smallToHigh);
		int GetIndex() const { return x360_->GetDeviceIndex(); }
		std::pair<uint64_t, ProControllerHid::InputStatus> GetLastInput() const;
		std::pair<uint64_t, ViGEm::X360OutputStatus> GetLastOutputIn() const;
		std::pair<uint64_t, ViGEm::X360OutputStatus> GetLastOutputOut() const;

	private:
		mutable std::mutex mutex_{};
		std::unique_ptr<ProControllerHid::ProController> controller_{};
		std::unique_ptr<ViGEm::X360Controller> x360_{};

		std::pair<int64_t, ProControllerHid::InputStatus> lastInput_{};
		std::pair<int64_t, ViGEm::X360OutputStatus> lastOutput_{};
		std::pair<int64_t, ViGEm::X360OutputStatus> lastOutputOut_{};

		mutable std::mutex lastInputMutex_;
		mutable std::mutex lastOutputMutex_;
		mutable std::mutex lastOutputOutMutex_;

		int largeMoterAmplification_{};
		int smallMoterAmplification_{};
		std::thread rumbleControlThread_{};
		std::atomic_flag rumbleControlThreadRunning_{ ATOMIC_FLAG_INIT };

		RumbleParams largeRumbleParam = { 0x80 , 16, 255 };
		RumbleParams smallRumbleParam = { 0x40, 16, 255 };

	private:
		void HandleControllerOutput(const ViGEm::X360OutputStatus& x360Output);
		void HandleControllerInput(const ProControllerHid::InputStatus& inputStatus);
		void RumbleControlTreadBody();
		static int64_t GetCurrentTimestamp();
	};

}
