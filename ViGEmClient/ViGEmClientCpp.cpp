#include "ViGEmClientCpp.h"
#include <wtypes.h>
#include <map>
#include <mutex>

#include "ViGEm/Client.h"

namespace ViGEm
{
	class ViGEmClientImpl final : public ViGEmClient, public std::enable_shared_from_this<ViGEmClientImpl>
	{
		PVIGEM_CLIENT client_{};
		VIGEM_ERROR error_{};
	public:
		bool IsConnected() const override { return error_ == VIGEM_ERROR_NONE; }

		ViGEmClientImpl()
		{
			client_ = vigem_alloc();
			error_ = vigem_connect(client_);
		}

		~ViGEmClientImpl() override
		{
			vigem_free(client_);
		}

		static std::shared_ptr<ViGEmClient> Connect()
		{
			return std::make_shared<ViGEmClientImpl>();
		}

		std::unique_ptr<X360Controller> AddX360Controller(
			std::function<void(const X360OutputStatus &status)> callback) override
		{
			class X360ControllerImpl : public X360Controller
			{
				std::function<void(const X360OutputStatus &status)> callback_{};
				std::shared_ptr<ViGEmClientImpl> client_{};
				PVIGEM_TARGET target_{};

				static void __stdcall NotificationCallbackHandler(
					PVIGEM_CLIENT Client, PVIGEM_TARGET Target,
					UCHAR LargeMotor, UCHAR SmallMotor, UCHAR LedNumber,
					LPVOID UserData)
				{
					if (auto p = static_cast<X360ControllerImpl*>(UserData))
					{
						if (p->callback_)
						{
							X360OutputStatus status{};
							status.largeRumble = LargeMotor;
							status.smallRumble = SmallMotor;
							status.ledNumber = LedNumber;
							p->callback_(status);
						}
					}
				}

			public:
				X360ControllerImpl(
					std::shared_ptr<ViGEmClientImpl> client,
					std::function<void(const X360OutputStatus &status)> callback)
					: callback_(std::move(callback))
					, client_(std::move(client))
					, target_(vigem_target_x360_alloc())
				{
					vigem_target_add(client_->client_, target_);
				}

				~X360ControllerImpl() override
				{
					vigem_target_x360_unregister_notification(target_);
					vigem_target_remove(client_->client_, target_);
					vigem_target_free(target_);
				}

				void Report(X360InputStatus inputStatus) override
				{
					int x = sizeof(XUSB_REPORT);
					int y = sizeof(X360InputStatus);
					static_assert(sizeof(XUSB_REPORT) == sizeof(X360InputStatus), "");
					vigem_target_x360_update(client_->client_, target_,
						*reinterpret_cast<XUSB_REPORT*>(&inputStatus));
				}

				unsigned long GetDeviceIndex() const override
				{
					return vigem_target_get_index(target_);
				}

				void StartNotification() override
				{
					vigem_target_x360_register_notification(client_->client_, target_, &NotificationCallbackHandler, this);
				}

				void StopNotification() override
				{
					vigem_target_x360_unregister_notification(target_);
				}
			};

			return std::make_unique<X360ControllerImpl>(shared_from_this(), callback);
		}
	};

	std::shared_ptr<ViGEmClient> ViGEmClient::Connect()
	{
		return std::make_shared<ViGEmClientImpl>();
	}
}
