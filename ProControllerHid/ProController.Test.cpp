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

#include <thread>
#include <mutex>

#include "hidio.h"
#include "ProController.h"

#define LF "\n\x1b[2K"

bool enable_imu_sensor = true;
bool enable_packet_dump = true;

static void SetupConsoleWindow();
static void WaitEscapeOrCtrlC();

static void logger_output(const std::string& text)
{
	static std::mutex console;
	std::lock_guard lock(console);
	std::cout << text + LF;
}

static size_t count_of_lines(std::string_view s)
{
	return std::count(s.begin(), s.end(), '\n');
}

using namespace ProControllerHid;

struct ControllerTest
{
	std::unique_ptr<ProController> controller{};
	InputStatus input{};
	RawInputStatus raw{};

	ControllerTest(std::unique_ptr<ProController> connected)
		: controller(std::move(connected))
	{
		// input status
		controller->SetInputStatusCallback([this](const InputStatus& input) { this->input = input; });

		// input to rumble/led output
		controller->SetRawInputStatusCallback([this](const RawInputStatus& raw)
		{
			this->raw = raw;

			// Rumble output
			auto lf = static_cast<uint8_t>(raw.LeftStick.AxisX >> 4);
			auto la = static_cast<uint8_t>(raw.LeftStick.AxisY >> 4);
			auto hf = static_cast<uint8_t>(raw.RightStick.AxisX >> 4);
			auto ha = static_cast<uint8_t>(raw.RightStick.AxisY >> 4);

			this->controller->SetRumbleBasic(
				raw.Buttons.LZButton ? la : 0,
				raw.Buttons.RZButton ? la : 0,
				raw.Buttons.LButton ? ha : 0,
				raw.Buttons.RButton ? ha : 0,
				lf, lf,
				hf, hf);

			// Player status led output
			auto led = static_cast<uint8_t>(
				raw.Buttons.AButton << 0 |
				raw.Buttons.BButton << 1 |
				raw.Buttons.XButton << 2 |
				raw.Buttons.YButton << 3);

			this->controller->SetPlayerLed(led);
		});
	}

	[[nodiscard]] std::string GetStatus() const
	{
		std::ostringstream output;
		output << "  Input >";
		output << " Clock=" << std::fixed << std::setprecision(3) << std::chrono::duration_cast<std::chrono::duration<double>>(raw.Timestamp.time_since_epoch()).count();
		output << " Report=" + DumpInputStatusAsString(raw) << LF;

		output << "  - Parsed > " << InputStatusAsString(raw) << LF;
		output << "  - Parsed > " << ImuSensorStatusAsString(raw) << LF;
		output << "  - Corrected > " << InputStatusAsString(input) << LF;
		output << "  - Corrected > " << ImuSensorStatusAsString(input) << LF;

		output << "  Output > Vibration Test (L/R Button): ";
		output << " lf/la=" << std::to_string(raw.LeftStick.AxisX >> 4) << "/" << std::to_string(raw.LeftStick.AxisY >> 4);
		output << " hf/ha=" << std::to_string(raw.RightStick.AxisX >> 4) << "/" << std::to_string(raw.RightStick.AxisY >> 4);
		output << LF;

		output << "  Output > LED Test (ABXY Button): ";
		output << (raw.Buttons.AButton ? "*" : "_");
		output << (raw.Buttons.BButton ? "*" : "_");
		output << (raw.Buttons.XButton ? "*" : "_");
		output << (raw.Buttons.YButton ? "*" : "_");
		output << LF;

		return output.str();
	}
};

int main()
{
	SetupConsoleWindow();

	std::vector<std::unique_ptr<ControllerTest>> controllers;

	std::cout << "Finding all ProControllers..." LF;

	auto devices = hidio::enumerate_devices(ProController::DeviceVendorID, ProController::DeviceProductID);
	for (size_t i = 0; i < devices.size(); i++)
	{
		const auto& device = devices[i];
		std::cout << "Device found: index = " << i << std::endl;
		std::cout << "  Path: " << device.device_path << std::endl;
		std::wcout << L"  Manufacture: " << device.manufacture_string << std::endl;
		std::wcout << L"  Product: " << device.product_string << std::endl;

		std::cout << "  Opening device..." << std::endl;
		if (auto controller = ProController::Connect(
			device.device_path.c_str(),
			enable_imu_sensor,
			[i](const char* text) { logger_output("device[" + std::to_string(i) + "]: " + text); },
			enable_packet_dump))
		{
			controllers.emplace_back(std::make_unique<ControllerTest>(std::move(controller)));
		}
		else
		{
			std::cout << "  Failed to open controller..." LF;
		}
	}

	if (controllers.empty())
	{
		std::cout << "No Pro Controllers found." LF;
		return 1;
	}
	std::cout << std::endl;

	// Start
	std::cout << "Controller starting..." LF LF;

	std::atomic_flag thread_running{};
	std::thread thread = std::thread([&]
	{
		thread_running.test_and_set();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		while (thread_running.test_and_set())
		{
			std::ostringstream output;
			output << LF;

			for (size_t i = 0; i < controllers.size(); i++)
			{
				output << "---- Controller " << i << LF << controllers[i]->GetStatus();
			}

			for (size_t i = 0, count = count_of_lines(output.str()); i < count + 1; i++)
			{
				output << "\x1b[1A"; // cursor move up
			}

			logger_output(output.str());
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	});

	WaitEscapeOrCtrlC();

	thread_running.clear();
	thread.join();

	std::cout << "Closing controllers..." << LF;
	controllers.clear();
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
