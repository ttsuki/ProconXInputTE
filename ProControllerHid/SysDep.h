#pragma once

namespace ProControllerHid
{
	namespace SysDep
	{
		void SetThreadPriorityToRealtime();
		void SetThreadName(const char* threadName);
	}
}
