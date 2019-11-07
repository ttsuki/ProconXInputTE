#pragma once

#include <string>
#include <ProControllerHid/ProController.h>

namespace ProconXInputTE
{
	namespace Tests
	{
		struct InputStatusString
		{
			const ProControllerHid::InputStatus input;
			InputStatusString(const ProControllerHid::InputStatus input) : input(input) {}
			std::string GetClockString() const;
			std::string GetRawDataString() const;
			std::string GetParsedInput() const;
			std::string GetParsedImu() const;
		};

		void SetupConsoleWindow();
		void WaitEscapeOrCtrlC();
	}
}
