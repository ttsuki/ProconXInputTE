#pragma once

#include <string>
#include <ProControllerHid/ProController.h>

namespace ProconXInputTE
{
	namespace Tests
	{
		std::string StatusString(
			const ProControllerHid::InputStatus& input,
			bool withClock, bool withRaw = false);

		void ResizeConsole();
		void WaitEscapeOrCtrlC();
	}
}