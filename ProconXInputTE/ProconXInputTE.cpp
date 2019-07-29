#include <iostream>
#include <vector>
#include <Windows.h>
#include <conio.h>

#include "../ProControllerHid/HidDeviceCollection.h"
#include "../ProControllerHid/ProController.h"
#include "../ViGEmClient/ViGEmClientCpp.h"

namespace ProconXInputTE
{
	using namespace ViGEm;

	constexpr unsigned short kNintendoVID{ 0x057E };
	constexpr unsigned short kProControllerPID{ 0x2009 };

	void RunProconTest()
	{
		std::unique_ptr<ProController> controller;
		for (auto device : HidDeviceCollection::EnumerateDevices(kNintendoVID, kProControllerPID))
		{
			std::cout << "Device found:" << std::endl;
			std::cout << "  Path: " << device.path << std::endl;
			std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
			std::wcout << L"  Product: " << device.product_string << std::endl;

			std::wcout << L"  Opening device..." << std::endl;

			controller = std::make_unique<ProController>(&device, 0, [&controller](const InputStatus& s)
				{
					const auto& lStick = s.LeftStick;
					const auto& rStick = s.RightStick;
					const auto& buttons = s.Buttons;

					printf(
						"\x1b[2K%lld: %9s"
						// "0x%06X, 0x%06X, 0x%06X = "
						" L(%4d,%4d)"
						" R(%4d,%4d)"
						" D:%s%s%s%s%s%s%s%s"
						" %s%s%s%s%s%s"
						" %s%s%s%s"
						"\r",
						s.clock, "Status>",
						//lStick.raw, rStick.raw, buttons.raw,

						lStick.AxisX, lStick.AxisY,
						rStick.AxisX, rStick.AxisY,

						buttons.LeftButton ? "L" : "_",
						buttons.RightButton ? "R" : "_",
						buttons.UpButton ? "U" : "_",
						buttons.DownButton ? "D" : "_",
						buttons.AButton ? "A" : "_",
						buttons.BButton ? "B" : "_",
						buttons.XButton ? "X" : "_",
						buttons.YButton ? "Y" : "_",

						buttons.LButton ? "L" : "_",
						buttons.RButton ? "R" : "_",
						buttons.LZButton ? "Lz" : "__",
						buttons.RZButton ? "Rz" : "__",
						buttons.LStick ? "Ls" : "__",
						buttons.RStick ? "Rs" : "__",

						buttons.PlusButton ? "+" : "_",
						buttons.MinusButton ? "-" : "_",
						buttons.HomeButton ? "H" : "_",
						buttons.ShareButton ? "S" : "_"
					);

					if (controller)
					{
						// Rumble output
						int lf = lStick.AxisX >> 4;
						int la = lStick.AxisY >> 4;
						int hf = rStick.AxisX >> 4;
						int ha = rStick.AxisY >> 4;
						controller->SetRumbleBasic(
							buttons.LZButton ? la : 0,
							buttons.RZButton ? la : 0,
							buttons.LButton ? ha : 0,
							buttons.RButton ? ha : 0,
							lf, lf, hf, hf);

						// player status led output
						controller->SetPlayerLed(
							buttons.AButton << 0 |
							buttons.BButton << 1 |
							buttons.XButton << 2 |
							buttons.YButton << 3);
					}
				});
			break;
		}

		if (controller)
		{
			std::cout << "Controller interaction started." << std::endl;
			std::cout << "Press Ctrl+C or ESCAPE to exit." << std::endl;
			while (int ch = _getch())
			{
				if (ch == 3 || ch == 27) { break; }
			}
			controller.reset();
			std::cout << std::endl;
			std::cout << "Closed." << std::endl;
		}
		else
		{
			std::cout << "No Pro Controllers found." << std::endl;
		}
	}

	class ProConX360Bridge
	{
		mutable std::mutex mutex_{};
		std::unique_ptr<ProController> controller_{};
		std::unique_ptr<X360Controller> x360_{};

		std::pair<int64_t, InputStatus> lastInput_{};
		std::pair<int64_t, X360OutputStatus> lastOutput_{};
		std::pair<int64_t, X360OutputStatus> lastOutput2_{};

		int largetMoterAmplification_{};
		int smallMoterAmplification_{};
		std::thread rumbleControlThread_{};
		std::atomic_flag rumbleControlThreadRunning_{ ATOMIC_FLAG_INIT };

		static int64_t Clock()
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
		}

	public:
		ProConX360Bridge(hid_device_info* proCon, ViGEmClient* client)
		{
			x360_ = client->AddX360Controller([this](const X360OutputStatus& x360Output) { HandleControllerOutput(x360Output); });
			controller_ = std::make_unique<ProController>(proCon, x360_->GetDeviceIndex(), [this](const InputStatus& proconInput) { HandleControllerInput(proconInput); });
			x360_->StartNotification();
			rumbleControlThreadRunning_.test_and_set();
			rumbleControlThread_ = std::thread([this] { RumbleControlTreadBody(); });
		}

		~ProConX360Bridge()
		{
			rumbleControlThreadRunning_.clear();
			rumbleControlThread_.join();
			x360_->StopNotification();
			controller_.reset();
			x360_.reset();
		}

		int GetIndex() const { return x360_->GetDeviceIndex(); }
		std::pair<uint64_t, InputStatus> GetLastInput() const { return lastInput_; }
		std::pair<uint64_t, X360OutputStatus> GetLastOutputIn() const { return lastOutput_; }
		std::pair<uint64_t, X360OutputStatus> GetLastOutputOut() const { return lastOutput2_; }

	private:
		void HandleControllerOutput(const X360OutputStatus& x360Output)
		{
			largetMoterAmplification_ = std::max<int>(largetMoterAmplification_, x360Output.largeRumble);
			smallMoterAmplification_ = std::max<int>(smallMoterAmplification_, x360Output.smallRumble);
			lastOutput_ = { Clock(), x360Output };
		}

		void RumbleControlTreadBody()
		{
			auto clock = std::chrono::steady_clock::now();
			while (rumbleControlThreadRunning_.test_and_set())
			{
				uint8_t large = (largetMoterAmplification_ = std::max<int>(largetMoterAmplification_ - 12, 0)) * 224 / 255;
				uint8_t small = (smallMoterAmplification_ = std::max<int>(smallMoterAmplification_ - 12, 0)) * 208 / 255;

				controller_->SetRumbleBasic(
					large, large, small, small,
					0x84, 0x84, 0x42, 0x42
				);
				lastOutput2_ = { Clock(), {large, small, 0} };

				clock += std::chrono::milliseconds(16);
				std::this_thread::sleep_until(clock);
			}
		}

		void HandleControllerInput(const InputStatus& inputStatus)
		{
			X360InputStatus status =
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
			lastInput_ = { Clock(), inputStatus };
			x360_->Report(status);
		}

	};

	void Run()
	{
		std::cout << "Starting ViGEm Client..." << std::endl;
		auto client = ViGEmClient::Connect();

		if (!client->IsConnected())
		{
			std::cout << "Failed to connect ViGEm Bus." << std::endl;
			std::cout << "Please install ViGEm Bus Driver." << std::endl;
			std::cout << "Please install ViGEm Bus Driver." << std::endl;
			return;
		}

		std::cout << "Finding Pro Controllers..." << std::endl;
		std::vector<std::unique_ptr<ProConX360Bridge>> bridges;
		for (auto device : HidDeviceCollection::EnumerateDevices(kNintendoVID, kProControllerPID))
		{
			std::cout << "Device found:" << std::endl;
			std::cout << "  Path: " << device.path << std::endl;
			std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
			std::wcout << L"  Product: " << device.product_string << std::endl;

			bridges.emplace_back(std::make_unique<ProConX360Bridge>(&device, client.get()));
			std::cout << "  Connected as Virtual X360 Controller index[" << bridges.back()->GetIndex() << "]" << std::endl;
		}

		if (bridges.empty())
		{
			std::cout << "No Pro Controllers found." << std::endl;
			return;
		}

		std::cout << "Controller interaction started." << std::endl;
		std::cout << "Press Ctrl+C or ESCAPE to exit." << std::endl;

		// monitor
		{
			std::atomic_flag exit{ ATOMIC_FLAG_INIT };
			exit.test_and_set();

			auto monitorThread = std::thread([&]
				{
					while (exit.test_and_set())
					{
						std::string message;
						for (auto&& b : bridges)
						{
							message += "\x1b[1A";
						}

						for (auto&& b : bridges)
						{
							char statusText[256];
							auto input = b->GetLastInput();
							auto output = b->GetLastOutputIn();
							auto outputReal = b->GetLastOutputOut();
							snprintf(statusText, sizeof(statusText),
								"\x1b[2KT%lld"
								" Out:L:%3d/%3d,H:%3d/%3d"
								" In:L(%4d,%4d),R(%4d,%4d)"
								",D:%s%s%s%s%s%s%s%s"
								"%s%s%s%s%s%s"
								"%s%s%s%s"
								"\n",
								input.first,

								outputReal.second.largeRumble, output.second.largeRumble,
								outputReal.second.smallRumble, output.second.smallRumble,
								input.second.LeftStick.AxisX, input.second.LeftStick.AxisY,
								input.second.RightStick.AxisX, input.second.RightStick.AxisY,

								input.second.Buttons.UpButton ? "U" : "",
								input.second.Buttons.DownButton ? "D" : "",
								input.second.Buttons.LeftButton ? "L" : "",
								input.second.Buttons.RightButton ? "R" : "",
								input.second.Buttons.AButton ? "A" : "",
								input.second.Buttons.BButton ? "B" : "",
								input.second.Buttons.XButton ? "X" : "",
								input.second.Buttons.YButton ? "Y" : "",

								input.second.Buttons.LButton ? "L" : "",
								input.second.Buttons.RButton ? "R" : "",
								input.second.Buttons.LZButton ? "Lz" : "",
								input.second.Buttons.RZButton ? "Rz" : "",
								input.second.Buttons.LStick ? "Ls" : "",
								input.second.Buttons.RStick ? "Rs" : "",

								input.second.Buttons.PlusButton ? "+" : "",
								input.second.Buttons.MinusButton ? "-" : "",
								input.second.Buttons.HomeButton ? "H" : "",
								input.second.Buttons.ShareButton ? "S" : ""
							);

							message += statusText;
						}
						std::cout << message;
						std::this_thread::sleep_for(std::chrono::milliseconds(16));
					}
				});

			while (int ch = _getch())
			{
				if (ch == 3 || ch == 27) { break; }
			}
			exit.clear();
			monitorThread.join();
		}

		std::cout << "Closing..." << std::endl;
		bridges.clear();
		std::cout << "Closed." << std::endl;
	}
}

int main()
{
	ProconXInputTE::Run();
}
