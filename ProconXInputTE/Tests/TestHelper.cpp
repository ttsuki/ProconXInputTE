#include "TestHelper.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
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
