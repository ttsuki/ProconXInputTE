// ViGEm Client RAII wrapper

#include "ViGEmClientCpp.h"
#include <Windows.h>

#include <mutex>
#include <stdexcept>

#include "ViGEm/Client.h"

namespace ViGEm
{
	using ViGEmClientPtr = std::shared_ptr<std::remove_pointer_t<PVIGEM_CLIENT>>;
	using ViGEmTargetPtr = std::shared_ptr<std::remove_pointer_t<PVIGEM_TARGET>>;

	struct ViGEmErrorException : std::runtime_error
	{
		VIGEM_ERROR e;
		explicit ViGEmErrorException(VIGEM_ERROR e) : std::runtime_error(GetErrorMessage(e)), e(e) { }

		static const char* GetErrorMessage(VIGEM_ERROR e)
		{
			switch (e)
			{
			//#define CASE(e) case e: return #e;
			case VIGEM_ERROR_NONE: return "VIGEM_ERROR_NONE";
			case VIGEM_ERROR_BUS_NOT_FOUND: return "VIGEM_ERROR_BUS_NOT_FOUND";
			case VIGEM_ERROR_NO_FREE_SLOT: return "VIGEM_ERROR_NO_FREE_SLOT";
			case VIGEM_ERROR_INVALID_TARGET: return "VIGEM_ERROR_INVALID_TARGET";
			case VIGEM_ERROR_REMOVAL_FAILED: return "VIGEM_ERROR_REMOVAL_FAILED";
			case VIGEM_ERROR_ALREADY_CONNECTED: return "VIGEM_ERROR_ALREADY_CONNECTED";
			case VIGEM_ERROR_TARGET_UNINITIALIZED: return "VIGEM_ERROR_TARGET_UNINITIALIZED";
			case VIGEM_ERROR_TARGET_NOT_PLUGGED_IN: return "VIGEM_ERROR_TARGET_NOT_PLUGGED_IN";
			case VIGEM_ERROR_BUS_VERSION_MISMATCH: return "VIGEM_ERROR_BUS_VERSION_MISMATCH";
			case VIGEM_ERROR_BUS_ACCESS_FAILED: return "VIGEM_ERROR_BUS_ACCESS_FAILED";
			case VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED: return "VIGEM_ERROR_CALLBACK_ALREADY_REGISTERED";
			case VIGEM_ERROR_CALLBACK_NOT_FOUND: return "VIGEM_ERROR_CALLBACK_NOT_FOUND";
			case VIGEM_ERROR_BUS_ALREADY_CONNECTED: return "VIGEM_ERROR_BUS_ALREADY_CONNECTED";
			case VIGEM_ERROR_BUS_INVALID_HANDLE: return "VIGEM_ERROR_BUS_INVALID_HANDLE";
			case VIGEM_ERROR_XUSB_USERINDEX_OUT_OF_RANGE: return "VIGEM_ERROR_XUSB_USERINDEX_OUT_OF_RANGE";
			case VIGEM_ERROR_INVALID_PARAMETER: return "VIGEM_ERROR_INVALID_PARAMETER";
			case VIGEM_ERROR_NOT_SUPPORTED: return "VIGEM_ERROR_NOT_SUPPORTED";
			}
			return "VIGEM_ERROR_UNKOWN";
		}
	};

	static void ThrowOnError(VIGEM_ERROR e)
	{
		if (e != VIGEM_ERROR_NONE)
		{
			if (IsDebuggerPresent()) DebugBreak();
			throw ViGEmErrorException(e);
		}
	}

	struct ViGEmClientImpl final : public ViGEmClient
	{
		ViGEmClientPtr client_;

		struct X360ControllerImpl final : public X360Controller
		{
			ViGEmClientPtr client_{};
			ViGEmTargetPtr target_{};

			std::mutex mutex_{};
			X360OutputStatus lastOutputStatus_{};
			X360OutputCallback callback_{};

		public:
			X360ControllerImpl(ViGEmClientPtr client)
				: client_(std::move(client))
				, target_(vigem_target_x360_alloc(), &vigem_target_free)
			{
				ThrowOnError(
					vigem_target_add(client_.get(), target_.get())
				);

				ThrowOnError(
					vigem_target_x360_register_notification(
						client_.get(), target_.get(), [](
						PVIGEM_CLIENT, PVIGEM_TARGET,
						UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber,
						LPVOID UserData)
						{
							X360OutputStatus status{};
							status.LargeRumble = LargeMotor;
							status.SmallRumble = SmallMotor;
							status.LedNumber = LedNumber;
							static_cast<X360ControllerImpl*>(UserData)->NotificationCallbackProc(status);
						}, this)
				);
			}

			void NotificationCallbackProc(X360OutputStatus output)
			{
				std::lock_guard<decltype(mutex_)> lock(mutex_);
				lastOutputStatus_ = output;
				if (callback_) callback_(lastOutputStatus_);
			}

			~X360ControllerImpl() override
			{
				StopNotification();
				vigem_target_x360_unregister_notification(target_.get());
				vigem_target_remove(client_.get(), target_.get());
			}

			unsigned long GetDeviceIndex() const override
			{
				return vigem_target_get_index(target_.get());
			}

			void SendReport(X360InputStatus inputStatus) override
			{
				static_assert(
					sizeof(XUSB_REPORT) == sizeof(X360InputStatus) &&
					std::is_trivial_v<XUSB_REPORT> &&
					std::is_trivial_v<X360InputStatus>,
					"type punning check failed.");
				XUSB_REPORT report{};
				memcpy(&report, &inputStatus, sizeof(report)); // type punning

				vigem_target_x360_update(client_.get(), target_.get(), report);
			}

			void ReceiveReport(X360OutputStatus* outputStatus) override
			{
				std::lock_guard<decltype(mutex_)> lock(mutex_);
				*outputStatus = lastOutputStatus_;
			}

			void StartNotification(X360OutputCallback callback) override
			{
				std::lock_guard<decltype(mutex_)> lock(mutex_);
				callback_ = callback;
			}

			void StopNotification() override
			{
				std::lock_guard<decltype(mutex_)> lock(mutex_);
				callback_ = nullptr;
			}
		};

	public:
		ViGEmClientImpl()
			: client_(ViGEmClientPtr(vigem_alloc(), &vigem_free))
		{
			ThrowOnError(
				vigem_connect(client_.get())
			);
		}

		std::unique_ptr<X360Controller> AddX360Controller() override
		{
			return std::make_unique<X360ControllerImpl>(client_);
		}
	};

	std::unique_ptr<ViGEmClient> ConnectToViGEm()
	{
		return std::make_unique<ViGEmClientImpl>();
	}
}
