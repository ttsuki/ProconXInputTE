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
		template <class U, class T, std::enable_if_t<std::is_trivially_copyable_v<T>, U> = 0>
		inline static U AsRawInt(T t)
		{
			union
			{
				T t_;
				U i_;
			} x{t};
			return x.i_;
		}

		std::string InputStatusString::GetClockString() const
		{
			char clockStr[32];
			snprintf(clockStr, sizeof(clockStr),
				"%lld",
				input.clock);
			return clockStr;
		}

		std::string InputStatusString::GetRawDataString() const
		{
			char rawInputStr[64];
			snprintf(rawInputStr, sizeof(rawInputStr),
				"%06X,%06X,%06X,%012llX,%012llX",
				AsRawInt<uint32_t>(input.LeftStick),
				AsRawInt<uint32_t>(input.RightStick),
				AsRawInt<uint32_t>(input.Buttons),
				AsRawInt<uint64_t>(input.Accelerometer),
				AsRawInt<uint64_t>(input.Gyroscope)
			);
			return rawInputStr;
		}

		std::string InputStatusString::GetParsedInput() const
		{
			char parsedStr[256];
			snprintf(parsedStr, sizeof(parsedStr),
				"L(%4d,%4d),R(%4d,%4d)"
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
			return parsedStr;
		}

		std::string InputStatusString::GetParsedImu() const
		{
			char imuStr[256]{};
			snprintf(imuStr, sizeof(imuStr),
				"Imu: Acl(%4d,%4d,%4d)/"
				"Gyr(%4d,%4d,%4d)",
				input.Accelerometer.X, input.Accelerometer.Y, input.Accelerometer.Z,
				input.Gyroscope.X, input.Gyroscope.Y, input.Gyroscope.Z);
			return imuStr;
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
