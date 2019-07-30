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
		template <class T, std::enable_if_t<std::is_trivially_copyable_v<T>, int> = 0>
		inline static int AsRaw32bit(T t)
		{
			union
			{
				T t_;
				int i_;
			} x{ t };
			return x.i_;
		}

		std::string StatusString(
			const ProControllerHid::InputStatus& input,
			bool withClock, bool withRaw)
		{
			char clockStr[32];
			snprintf(clockStr, sizeof(clockStr),
				"%lld:",
				input.clock);

			char rawInputStr[32];
			snprintf(rawInputStr, sizeof(rawInputStr),
				"0x%06X,0x%06X,0x%06X:",
				AsRaw32bit(input.LeftStick),
				AsRaw32bit(input.RightStick),
				AsRaw32bit(input.Buttons)
			);

			char statusText[256];
			snprintf(statusText, sizeof(statusText),
				"%s%s"
				"L(%4d,%4d),R(%4d,%4d)"
				",D:%s%s%s%s%s%s%s%s"
				"%s%s%s%s%s%s"
				"%s%s%s%s",
				withClock ? clockStr : "",
				withRaw ? rawInputStr : "",

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
			return std::string(statusText);
		}

		void ResizeConsole()
		{
			CONSOLE_SCREEN_BUFFER_INFO info;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
			info.dwSize.X = std::max(info.dwSize.X, SHORT{ 120 });
			SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), info.dwSize);
		}

		void WaitEscapeOrCtrlC()
		{
			std::cout << "Press Ctrl+C or ESCAPE to exit." << std::endl;
			while (int ch = _getch())
			{
				if (ch == 3 || ch == 27) { break; }
			}
		}
	}
}
