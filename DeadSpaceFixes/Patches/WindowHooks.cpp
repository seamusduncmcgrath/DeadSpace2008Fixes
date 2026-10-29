#include "common.h"
#include "Utils.h"

namespace Input {

	namespace RemoveWindowsHooks {

		SetWindowsHookExA_t oSetWindowsHookExA = nullptr;

		HHOOK WINAPI hkSetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId)
		{
			if (idHook == WH_KEYBOARD_LL)
			{
				LOG_INFO("[Fixes/Input]", "Blocked game from installing input hook");
				return NULL;
			}
			return oSetWindowsHookExA(idHook, lpfn, hmod, dwThreadId);
		}
		
		void HookSetWindowsHookExA()
		{
			HMODULE hUser32 = GetModuleHandleA("user32.dll");
			if (hUser32)
			{
				LPVOID pSetWindowsHookExA = GetProcAddress(hUser32, "SetWindowsHookExA");
				if (pSetWindowsHookExA)
				{
					MH_CreateHook(pSetWindowsHookExA, &hkSetWindowsHookExA, reinterpret_cast<LPVOID*>(&oSetWindowsHookExA));
					MH_EnableHook(pSetWindowsHookExA);
				}
			}
		}

		void Apply(HMODULE hExe)
		{
			HookSetWindowsHookExA();
		}
	}
}
