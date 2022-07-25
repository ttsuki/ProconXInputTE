#include <iomanip>
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <iostream>
#include <sstream>

#include <ProControllerHid/hidio.h>
#include <ProControllerHid/ProController.h>

#include "TestHelper.h"
#define CLEAR_LINE "\x1b[2K"

namespace ProconXInputTE
{
	namespace Tests
	{
		bool enable_imu_sensor = true;
		bool enable_packet_dump = true;

		void logger_output(const std::string& text)
		{
			static std::mutex m;
			std::lock_guard lock(m);
			std::cerr << (CLEAR_LINE + text + "\n");
		}

		void RunProControllerDriverTest()
		{
			using namespace ProControllerHid;

			SetupConsoleWindow();

			std::mutex console;
			

			struct ControllerTest
			{
				int index{};
				std::unique_ptr<ProController> controller{};
				InputStatus input{};
				RawInputStatus raw{};
			};

			std::vector<std::unique_ptr<ControllerTest>> controllers;

			int index = 0;
			for (const auto &device : hidio::enumerate_devices(ProController::DeviceVendorID, ProController::DeviceProductID))
			{
				auto entry = std::make_unique<ControllerTest>();
				entry->index = index;

				std::cout << "Device found: index = " << index << std::endl;
				std::cout << "  Path: " << device.device_path << std::endl;
				std::wcout << L"  Manufacture: " << device.manufacture_string << std::endl;
				std::wcout << L"  Product: " << device.product_string << std::endl;

				std::wcout << L"  Opening device..." << std::endl;
				entry->controller = ProController::Connect(
					device.device_path.c_str(),
					enable_imu_sensor,
					[index](const char* text) { logger_output(std::to_string(index) + ": " + text); },
					enable_packet_dump);

				if (!entry->controller)
				{
					std::wcout << L"  Failed to open controller..." << std::endl;
					continue;
				}

				entry->controller->SetInputStatusCallback([e = entry.get()](const InputStatus& input)
				{
					e->input = input;
				});

				entry->controller->SetRawInputStatusCallback([e = entry.get(), controller = entry->controller.get()](const RawInputStatus& raw)
				{
					e->raw = raw;

					// Rumble output
					auto lf = raw.LeftStick.AxisX >> 4;
					auto la = raw.LeftStick.AxisY >> 4;
					auto hf = raw.RightStick.AxisX >> 4;
					auto ha = raw.RightStick.AxisY >> 4;

					controller->SetRumbleBasic(
						raw.Buttons.LZButton ? la : 0,
						raw.Buttons.RZButton ? la : 0,
						raw.Buttons.LButton ? ha : 0,
						raw.Buttons.RButton ? ha : 0,
						lf, lf, hf, hf);

					// Player status led output
					auto led = raw.Buttons.AButton << 0
						| raw.Buttons.BButton << 1
						| raw.Buttons.XButton << 2
						| raw.Buttons.YButton << 3;

					controller->SetPlayerLed(led);
				});


				controllers.emplace_back(std::move(entry));
				index++;
			}

			if (controllers.empty())
			{
				std::cout << "No Pro Controllers found." << std::endl;
				return;
			}

			std::cout << std::endl;
			std::cout << "Controller starting...\n" << std::endl;

			std::atomic_flag running{};
			running.test_and_set();
			std::thread thread = std::thread([&]
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					while (running.test_and_set())
					{
						std::ostringstream output;
						int line_count = 0;

						output << CLEAR_LINE << "\n", line_count++;
						for (auto& c : controllers)
						{
							auto clock = std::chrono::duration_cast<std::chrono::duration<double>>(c->raw.Timestamp.time_since_epoch()).count();
							output << CLEAR_LINE "---- Controller " << c->index << "\n", line_count++;
							output << CLEAR_LINE "  Input > Clock=" << std::fixed << std::setprecision(3) << clock << " Report=" + DumpInputStatusAsString(c->raw) << "\n", line_count++;
							output << CLEAR_LINE "  - Parsed > " << InputStatusAsString(c->raw) << "\n", line_count++;
							output << CLEAR_LINE "  - Parsed > " << ImuSensorStatusAsString(c->raw) << "\n", line_count++;
							output << CLEAR_LINE "  - Corrected > " << InputStatusAsString(c->input) << "\n", line_count++;
							output << CLEAR_LINE "  - Corrected > " << ImuSensorStatusAsString(c->input) << "\n", line_count++;

							output << CLEAR_LINE "  Output > Vibration Test (L/R Button): ";
							output << " lf/la=" << std::to_string(c->raw.LeftStick.AxisX >> 4) << "/" << std::to_string(c->raw.LeftStick.AxisY >> 4);
							output << " hf/ha=" << std::to_string(c->raw.RightStick.AxisX >> 4) << "/" << std::to_string(c->raw.RightStick.AxisY >> 4);
							output << "\n", line_count++;

							output << CLEAR_LINE "  Output > LED Test (ABXY Button): ";
							output << (c->raw.Buttons.AButton ? "*" : "_");
							output << (c->raw.Buttons.BButton ? "*" : "_");
							output << (c->raw.Buttons.XButton ? "*" : "_");
							output << (c->raw.Buttons.YButton ? "*" : "_");
							output << "\n", line_count++;
						}

						for (int i = 0; i < line_count + 1; i++)
						{
							output << "\x1b[1A";
						}

						logger_output(output.str());
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
					}
				});

			WaitEscapeOrCtrlC();
			running.clear();
			thread.join();

			controllers.clear();
			std::cout << "Closed." << std::endl;
		}
	}
}
