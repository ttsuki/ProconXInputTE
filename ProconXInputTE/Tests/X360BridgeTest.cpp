#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <memory>
#include <vector>
#include <string>
#include <mutex>

#include <atomic>
#include <thread>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <ProControllerHid/ProController.h>
#include <ViGEmClient/ViGEmClientCpp.h>

#include "TestHelper.h"

#include "../ProconX360Bridge.h"

namespace ProconXInputTE
{
	namespace Tests
	{
		void RunX360BridgeTest()
		{
			using namespace ProControllerHid;
			using namespace ViGEm;

			SetupConsoleWindow();

			std::cout << "Starting ViGEm Client..." << std::endl;
			std::unique_ptr<ViGEmClient> client;
			try
			{
				client = ViGEm::ConnectToViGEm();
			}
			catch (const std::runtime_error& re)
			{
				std::cout << "Failed to connect ViGEm Bus. ERROR=" << re.what() << std::endl;
				std::cout << "Please make sure ViGEm Bus Driver installed." << std::endl;
				return;
			}

			std::cout << "Finding Pro Controllers..." << std::endl;
			std::vector<std::unique_ptr<ProconX360Bridge>> bridges;
			for (const auto& devPath : ProController::EnumerateProControllerDevicePaths())
			{
				std::cout << "- Device found:" << std::endl;
				std::cout << "  Path: " << devPath << std::endl;
				bridges.emplace_back(std::make_unique<ProconX360Bridge>(devPath.c_str(), client.get()));
				std::cout << "  Connected as Virtual X360 Controller" << " index[" << bridges.back()->GetIndex() << "]" << std::endl;
			}

			if (bridges.empty())
			{
				std::cout << "No Pro Controllers found." << std::endl;
				return;
			}

			std::cout << std::endl;
			std::cout << "Controller Bridges started." << std::endl;

			// monitor
			{
				std::atomic_flag monitorThreadRunning{};
				monitorThreadRunning.test_and_set();

				std::mutex console;
				auto monitorThread = std::thread([&]()
				{
					SetThreadDescription(GetCurrentThread(), L"MonitorThread");
					while (monitorThreadRunning.test_and_set())
					{
						std::stringstream output;

						for (auto&& b : bridges)
						{
							auto input = b->GetLastInput().second; // controller input
							auto outputIn = b->GetLastOutputIn().second; // x360 input value
							auto outputOut = b->GetLastOutputOut().second; // sent to controller value

							output << "\x1b[2K"; // Clear this lline
							output << b->GetIndex() << ">";
							output << "Fb";
							output << " L:"
								<< std::setw(3) << static_cast<int>(outputOut.largeRumble) << "/"
								<< std::setw(3) << static_cast<int>(outputIn.largeRumble);
							output << " H:"
								<< std::setw(3) << static_cast<int>(outputOut.smallRumble) << "/"
								<< std::setw(3) << static_cast<int>(outputIn.smallRumble);
							output << "  In " << InputStatusAsString(input);
							output << "\n";
						}

						for ([[maybe_unused]] auto&& _ : bridges)
						{
							output << "\x1b[1A";
						}

						std::cout << output.str() << std::flush;
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
					}
				});

				WaitEscapeOrCtrlC();

				monitorThreadRunning.clear();
				monitorThread.join();
			}

			std::cout << std::string(bridges.size() + 1, '\n');
			std::cout << "Closing..." << std::endl;
			bridges.clear();
			std::cout << "Closed." << std::endl;
		}
	}
}
