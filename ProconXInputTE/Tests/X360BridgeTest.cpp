#include <memory>
#include <vector>
#include <string>
#include <mutex>

#include <atomic>
#include <thread>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <Windows.h>

#include <hidapi.h>
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
			auto client = ViGEmClient::Connect();

			if (!client->IsConnected())
			{
				std::cout << "Failed to connect ViGEm Bus." << std::endl;
				std::cout << "Please install ViGEm Bus Driver." << std::endl;
				std::cout << "Please install ViGEm Bus Driver." << std::endl;
				return;
			}

			std::cout << "Finding Pro Controllers..." << std::endl;
			std::vector<std::unique_ptr<ProconX360Bridge>> bridges;
			for (auto device : EnumerateProControllers())
			{
				std::cout << "- Device found:" << std::endl;
				std::cout << "  Path: " << device.path << std::endl;
				std::wcout << L"  Manufacture: " << device.manufacturer_string << std::endl;
				std::wcout << L"  Product: " << device.product_string << std::endl;

				bridges.emplace_back(std::make_unique<ProconX360Bridge>(&device, client.get()));
				std::cout << "  Connected as Virtual X360 Controller"
					<< " index[" << bridges.back()->GetIndex() << "]"
					<< std::endl;
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
				std::atomic_flag monitorThreadRunning{ATOMIC_FLAG_INIT};
				monitorThreadRunning.test_and_set();

				std::mutex console;
				auto monitorThread = std::thread([&]()
				{
					SetThreadDescription(GetCurrentThread(), L"MonitorThread");
					while (monitorThreadRunning.test_and_set())
					{
						std::stringstream message;
						for (auto &&b : bridges)
						{
							auto input = b->GetLastInput().second;
							auto outputIn = b->GetLastOutputIn().second;
							auto outputOut = b->GetLastOutputOut().second;
							message << "\x1b[2K";
							message << b->GetIndex() << ">";
							message << "Fb";
							message << " L:"
								<< std::setw(3) << static_cast<int>(outputOut.largeRumble) << "/"
								<< std::setw(3) << static_cast<int>(outputIn.largeRumble);
							message << " H:"
								<< std::setw(3) << static_cast<int>(outputOut.smallRumble) << "/"
								<< std::setw(3) << static_cast<int>(outputIn.smallRumble);
							message << "  In " << StatusString(input, false, false);
							message << "\n";
						}

						for (auto &&b : bridges)
						{
							message << "\x1b[1A";
						}
						auto s = message.str();
						std::cout << s << std::flush;
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
