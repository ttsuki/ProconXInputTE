#include "SysDep.h"

#define WIN32_LEAN_AND_MEANS
#include <Windows.h>
#include <string>

namespace ProControllerHid
{
	namespace SysDep
	{
		void SetThreadPriorityToRealtime()
		{
			::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
		}

		void SetThreadName(const char *threadName)
		{
			SetThreadDescription(::GetCurrentThread(),
				std::wstring(threadName, threadName + strlen(threadName)).c_str());
		}
	}
}
