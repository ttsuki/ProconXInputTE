#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <conio.h>
#endif

#include <memory>
#include <vector>
#include <string>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <atomic>
#include <thread>
#include <mutex>

#include <ProControllerHid/ProController.h>
#include <ViGEmClient/ViGEmClientCpp.h>

#include "ProconX360Bridge.h"

#define LF "\n\x1b[2K"

static void SetupConsoleWindow();
static void WaitEscapeOrCtrlC();

int main([[maybe_unused]] int argc, [[maybe_unused]] const char* argv[])
{
	SetupConsoleWindow();

	using namespace ProControllerHid;
	using namespace ViGEm;
	using namespace ProconXInputTE;

	std::cout << "Starting ViGEm Client..." << LF;
	std::unique_ptr<ViGEmClient> client;
	try
	{
		client = ViGEm::ConnectToViGEm();
	}
	catch (const std::runtime_error& re)
	{
		std::cout << "Failed to connect ViGEm Bus. ERROR=" << re.what() << LF;
		std::cout << "Please make sure ViGEm Bus Driver installed." << LF;
		return 1;
	}
	std::cout << "ViGEm Client Ready." << LF;

	std::cout << "Finding all ProControllers..." LF;

	auto devices = ProController::EnumerateProControllerDevicePaths();
	std::vector<std::unique_ptr<ProconX360Bridge>> bridges;
	for (size_t i = 0; i < devices.size(); i++)
	{
		const auto device = devices[i];
		std::cout << "- Device found:" << LF;
		std::cout << "  Path: " << device << LF;
		bridges.emplace_back(std::make_unique<ProconX360Bridge>(device.c_str(), client.get()));
		std::cout << "  Connected as Virtual X360 Controller" << " index[" << bridges.back()->GetIndex() << "]" << LF;
	}

	if (bridges.empty())
	{
		std::cout << "No Pro Controllers found." << LF;
		return 1;
	}

	std::cout << LF;
	std::cout << "Controller Bridges started." << LF;

	// monitor
	std::atomic_flag monitor_thread_running{};
	auto monitor_thread = std::thread([&]()
	{
		monitor_thread_running.test_and_set();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		while (monitor_thread_running.test_and_set())
		{
			std::stringstream output;

			output << LF;
			for (auto&& b : bridges)
			{
				auto input = b->GetLastInput().second;         // controller input
				auto outputIn = b->GetLastOutputIn().second;   // x360 input value
				auto outputOut = b->GetLastOutputOut().second; // sent to controller value

				output << b->GetIndex() << ">";
				output << "Vib";
				output << " L:"
					<< std::setw(3) << static_cast<int>(outputOut.LargeRumble) << "/"
					<< std::setw(3) << static_cast<int>(outputIn.LargeRumble);
				output << " H:"
					<< std::setw(3) << static_cast<int>(outputOut.SmallRumble) << "/"
					<< std::setw(3) << static_cast<int>(outputIn.SmallRumble);
				output << "  In " << InputStatusAsString(input);
				output << LF;
			}

			for (size_t i = 0; i < bridges.size() + 2; ++i)
			{
				output << "\x1b[1A"; // cursor move up
			}

			std::cout << output.str() << LF;

			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}
	});

	WaitEscapeOrCtrlC();

	monitor_thread_running.clear();
	monitor_thread.join();

	std::cout << "Closing controllers..." << LF;
	bridges.clear();
	std::cout << "Closed." << LF;
}

static void SetupConsoleWindow()
{
#ifdef _WIN32
	DWORD mode = {};
	GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
	SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	CONSOLE_SCREEN_BUFFER_INFO info;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
	info.dwSize.X = std::max(info.dwSize.X, SHORT{120});
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), info.dwSize);
#endif
}

static void WaitEscapeOrCtrlC()
{
#ifdef _WIN32
	std::cout << "Press Ctrl+C or ESCAPE to exit.\n" << std::endl;
	while (int ch = _getch())
	{
		if (ch == 3 || ch == 27) { break; }
	}
#else
	std::cout << "Press Enter to exit.\n" << std::endl;
	(void)getchar();
#endif
}
