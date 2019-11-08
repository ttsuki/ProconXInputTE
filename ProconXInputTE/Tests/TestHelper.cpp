#include "TestHelper.h"

#include <Windows.h>
#include <conio.h>

#include <type_traits>
#include <algorithm>
#include <iostream>
#include <ProControllerHid/ProController.h>

namespace ProconXInputTE
{
	namespace Tests
	{
		template <class U, class T, std::enable_if_t<std::is_trivially_copyable_v<T>, U>  = 0>
		inline static U AsRawInt(T t)
		{
			union
			{
				T t_;
				U i_;
			} x{t};
			return x.i_;
		}

		InputStatusString::InputStatusString(const ProControllerHid::InputStatus &input)
			: input(input)
		{
		}

		InputStatusString::InputStatusString(
			const ProControllerHid::InputStatus &input,
			const ProControllerHid::CorrectedInputStatus corrected)
			: input(input)
			, corrected(corrected)
		{
		}

		std::string InputStatusString::GetClockString() const
		{
			char str[32];
			snprintf(str, sizeof(str),
				"%lld",
				input.clock);
			return str;
		}

		std::string InputStatusString::GetRawDataString() const
		{
			char str[64];
			snprintf(str, sizeof(str),
				"%06X,%06X,%06X,%012llX,%012llX",
				AsRawInt<uint32_t>(input.LeftStick),
				AsRawInt<uint32_t>(input.RightStick),
				AsRawInt<uint32_t>(input.Buttons),
				AsRawInt<uint64_t>(input.Sensors[0].Accelerometer),
				AsRawInt<uint64_t>(input.Sensors[0].Gyroscope)
			);
			return str;
		}

		std::string InputStatusString::GetParsedInput() const
		{
			char str[256];
			snprintf(str, sizeof(str),
				"L(%4u,%4u),R(%4u,%4u)"
				",Buttons:%s%s%s%s%s%s%s%s"
				"%s%s%s%s%s%s"
				"%s%s%s%s",

				input.LeftStick.AxisX, input.LeftStick.AxisY,
				input.RightStick.AxisX, input.RightStick.AxisY,

				input.Buttons.UpButton ? "U" : "",
				input.Buttons.DownButton ? "D" : "",
				input.Buttons.LeftButton ? "L" : "",
				input.Buttons.RightButton ? "R" : "",
				input.Buttons.AButton ? "A" : "",
				input.Buttons.BButton ? "B" : "",
				input.Buttons.XButton ? "X" : "",
				input.Buttons.YButton ? "Y" : "",

				input.Buttons.LButton ? "L" : "",
				input.Buttons.RButton ? "R" : "",
				input.Buttons.LZButton ? "Lz" : "",
				input.Buttons.RZButton ? "Rz" : "",
				input.Buttons.LStick ? "Ls" : "",
				input.Buttons.RStick ? "Rs" : "",

				input.Buttons.PlusButton ? "+" : "",
				input.Buttons.MinusButton ? "-" : "",
				input.Buttons.HomeButton ? "H" : "",
				input.Buttons.ShareButton ? "S" : ""
			);
			return str;
		}

		std::string InputStatusString::GetParsedImu() const
		{
			char str[256];
			const auto &sensor = input.Sensors[0];
			snprintf(str, sizeof(str),
				"Imu: Acl(%4d,%4d,%4d)/Gyr(%4d,%4d,%4d)",
				sensor.Accelerometer.X, sensor.Accelerometer.Y, sensor.Accelerometer.Z,
				sensor.Gyroscope.X, sensor.Gyroscope.Y, sensor.Gyroscope.Z);
			return str;
		}

		std::string InputStatusString::GetCorrectedInput() const
		{
			char str[256];
			snprintf(str, sizeof(str),
				"L(%+.3f,%+.3f),R(%+.3f,%+.3f)",
				corrected.LeftStick.X, corrected.LeftStick.Y,
				corrected.RightStick.X, corrected.RightStick.Y
			);
			return str;
		}

		std::string InputStatusString::GetCorrectedImu() const
		{
			char str[256];
			const auto &sensor = corrected.Sensors[0];
			snprintf(str, sizeof(str),
				"Imu: Acl(%+.4f,%+.4f,%+.4f)/Gyr(%+.4f,%+.4f,%+.4f)",
				sensor.Accelerometer.X, sensor.Accelerometer.Y, sensor.Accelerometer.Z,
				sensor.Gyroscope.X, sensor.Gyroscope.Y, sensor.Gyroscope.Z);
			return str;
		}

		void SetupConsoleWindow()
		{
			DWORD mode = {};
			GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
			SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

			CONSOLE_SCREEN_BUFFER_INFO info;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
			info.dwSize.X = std::max(info.dwSize.X, SHORT{120});
			SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), info.dwSize);
		}

		void WaitEscapeOrCtrlC()
		{
			std::cout << "Press Ctrl+C or ESCAPE to exit.\n" << std::endl;
			while (int ch = _getch())
			{
				if (ch == 3 || ch == 27) { break; }
			}
		}
	}
}
