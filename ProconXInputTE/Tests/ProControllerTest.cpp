#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <iostream>

#include <ProControllerHid/ProController.h>

#include "TestHelper.h"

namespace ProconXInputTE
{
	namespace Tests
	{
		constexpr int StatusLines = 8;

		void RunProconTest()
		{
			using namespace ProControllerHid;
			SetupConsoleWindow();

			std::mutex console;
			std::vector<std::unique_ptr<ProController>> controllers;
			int index = 0;

			for (const auto &devPath : ProController::EnumerateProControllerDevicePaths())
			{
				std::cout << "Device found:" << std::endl;
				std::cout << "  Path: " << devPath << std::endl;
				//std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
				//std::wcout << L"  Product: " << device.product_string << std::endl;

				std::wcout << L"  Opening device..." << std::endl;
				auto callback = [&controllers, index, &console](const InputStatus &s)
				{
					auto &controller = controllers[index];

					// Rumble output
					int lf = s.LeftStick.AxisX >> 4;
					int la = s.LeftStick.AxisY >> 4;
					int hf = s.RightStick.AxisX >> 4;
					int ha = s.RightStick.AxisY >> 4;

					int led = s.Buttons.AButton << 0
						| s.Buttons.BButton << 1
						| s.Buttons.XButton << 2
						| s.Buttons.YButton << 3;

					std::string line(index * StatusLines, '\n');
					InputStatusString st = InputStatusString(s, controller->CorrectInput(s));

					line += "\x1b[2K" "---- Controller " + std::to_string(index) + "\n";
					line += "\x1b[2K" "  Input > Clock=" + st.GetClockString() + " Report=" + st.GetRawDataString() + "\n";
					line += "\x1b[2K" "  - Parsed > " + st.GetParsedInput() + "\n";
					line += "\x1b[2K" "  - Parsed > " + st.GetParsedImu() + "\n";
					line += "\x1b[2K" "  - Corrected > " + st.GetCorrectedInput() + "\n";
					line += "\x1b[2K" "  - Corrected > " + st.GetCorrectedImu() + "\n";

					line += "\x1b[2K" "  Output > Vibration Test (L/R Button): ";
					line += " lf/la=" + std::to_string(lf) + "/" + std::to_string(la);
					line += " hf/ha=" + std::to_string(hf) + "/" + std::to_string(ha);
					line += "\n";

					line += "\x1b[2K" "  Output > LED Test (ABXY Button): ";
					line += led & 1 ? "*" : "_";
					line += led & 2 ? "*" : "_";
					line += led & 4 ? "*" : "_";
					line += led & 8 ? "*" : "_";
					line += "\n";

					for (int i = 0; i < (index + 1) * StatusLines; i++)
					{
						line += "\x1b[1A";
					}

					{
						std::lock_guard<std::mutex> lock(console);
						std::cout << line << std::flush;
					}

					controller->SetRumbleBasic(
						s.Buttons.LZButton ? la : 0,
						s.Buttons.RZButton ? la : 0,
						s.Buttons.LButton ? ha : 0,
						s.Buttons.RButton ? ha : 0,
						lf, lf, hf, hf);

					// player status led output
					controller->SetPlayerLed(led);
				};

				controllers.emplace_back(ProController::Connect(devPath.c_str(), index + 1, callback, true));
				index++;
			}

			if (controllers.empty())
			{
				std::cout << "No Pro Controllers found." << std::endl;
				return;
			}

			std::cout << std::endl;
			std::cout << "Controller starting...\n" << std::endl;
			for (auto &&controller : controllers)
			{
				controller->StartStatusCallback();
			}
			WaitEscapeOrCtrlC();
			for (auto &&controller : controllers)
			{
				controller->StopStatusCallback();
			}

			std::cout << std::string(controllers.size() * StatusLines + 1, '\n');
			controllers.clear();
			std::cout << "Closed." << std::endl;
		}
	}
}
