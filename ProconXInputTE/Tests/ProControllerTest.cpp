#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <iostream>

#include <hidapi.h>
#include <ProControllerHid/ProController.h>

#include "TestHelper.h"

namespace ProconXInputTE
{
	namespace Tests
	{
		void RunProconTest()
		{
			using namespace ProControllerHid;
			SetupConsoleWindow();

			std::mutex console;
			std::vector<std::unique_ptr<ProController>> controllers;
			int index = 0;

			for (auto device : EnumerateProControllers())
			{
				std::cout << "Device found:" << std::endl;
				std::cout << "  Path: " << device.path << std::endl;
				std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
				std::wcout << L"  Product: " << device.product_string << std::endl;

				std::wcout << L"  Opening device..." << std::endl;
				auto callback = [&controllers, index, &console](const InputStatus &s)
				{
					auto &controller = controllers[index];

					// Rumble output
					int lf = s.LeftStick.AxisX >> 4;
					int la = s.LeftStick.AxisY >> 4;
					int hf = s.RightStick.AxisX >> 4;
					int ha = s.RightStick.AxisY >> 4;

					std::string line(index, '\n');
					line += "\x1b[2K";
					line += std::to_string(index);
					line += ">";
					line += "Fb";
					line += " lf/la=" + std::to_string(lf) + "/" + std::to_string(la);
					line += " hf/ha=" + std::to_string(hf) + "/" + std::to_string(ha);

					line += StatusString(s, true, true);
					line += "\r";
					for (int i = 0; i < index; i++)
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
					controller->SetPlayerLed(
						s.Buttons.AButton << 0 |
						s.Buttons.BButton << 1 |
						s.Buttons.XButton << 2 |
						s.Buttons.YButton << 3);
				};

				controllers.emplace_back(ProController::Connect(&device, index + 1, callback));
				index++;
			}

			if (controllers.empty())
			{
				std::cout << "No Pro Controllers found." << std::endl;
				return;
			}

			std::cout << std::endl;
			std::cout << "Controller started." << std::endl;
			for (auto &&controller : controllers)
			{
				controller->StartStatusCallback();
			}
			WaitEscapeOrCtrlC();
			for (auto &&controller : controllers)
			{
				controller->StopStatusCallback();
			}

			std::cout << std::string(controllers.size() + 1, '\n');
			controllers.clear();
			std::cout << "Closed." << std::endl;
		}
	}
}
