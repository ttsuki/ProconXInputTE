#include <iostream>
#include <Windows.h>

#include "ProconIO/HidDeviceCollection.h"
#include "ProconIO/ProController.h"

namespace ProconXInputTE
{
	constexpr unsigned short kNintendoVID{ 0x057E };
	constexpr unsigned short kProControllerPID{ 0x2009 };

	void Run()
	{
		std::unique_ptr<ProController> controller;
		InputStatus inputStatus = {};
		for (auto device : HidDeviceCollection::EnumerateDevices(kNintendoVID, kProControllerPID))
		{
			std::cout << "Device found:" << std::endl;
			std::cout << "  Path: " << device.path << std::endl;
			std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
			std::wcout << L"  Product: " << device.product_string << std::endl;

			std::wcout << L"  Opening device..." << std::endl;

			controller = std::make_unique<ProController>(&device, 0, [&controller, &inputStatus](const InputStatus& s)
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
							buttons.RButton ? ha : 0
						); // ,lf, lf, hf, hf);

						// player status led output
						controller->SetPlayerLed(
							buttons.AButton << 0 |
							buttons.BButton << 1 |
							buttons.XButton << 2 |
							buttons.YButton << 3);
					}
				});
		}

		if (controller)
		{
			int playerLed = 0;
			while (true)
			{
				if (GetAsyncKeyState(VK_ESCAPE)) { break; }
				Sleep(16);
			}
		}
		controller.reset();
	}
}

int main()
{
	ProconXInputTE::Run();
}
