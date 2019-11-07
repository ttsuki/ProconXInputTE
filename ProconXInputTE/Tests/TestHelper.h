#pragma once

#include <string>
#include <ProControllerHid/ProController.h>

namespace ProconXInputTE
{
	namespace Tests
	{
		struct InputStatusString
		{
			const ProControllerHid::InputStatus input{};
			const ProControllerHid::CorrectedInputStatus corrected{};
			InputStatusString(const ProControllerHid::InputStatus &input);
			InputStatusString(const ProControllerHid::InputStatus &input, const ProControllerHid::CorrectedInputStatus corrected);
			std::string GetClockString() const;
			std::string GetRawDataString() const;
			std::string GetParsedInput() const;
			std::string GetParsedImu() const;
			std::string GetCorrectedInput() const;
			std::string GetCorrectedImu() const;
		};

		void SetupConsoleWindow();
		void WaitEscapeOrCtrlC();
	}
}
