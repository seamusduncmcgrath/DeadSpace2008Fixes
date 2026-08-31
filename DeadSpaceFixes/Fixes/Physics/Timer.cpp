#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Physics {

		namespace Timer {

			void Apply(HMODULE hExe)
			{
				const char* timerSignature = "80 3D ? ? ? ? 00 74 15 8D 54 24 0C 52";
				uintptr_t patternAddress = Utils::FindPattern(hExe, timerSignature);

				// High precision timer fix: can fix some of the issues with high framerates.
				// Should still cap to 120-180 max, as at 200-300 the issues come back.
				// The reason this works is the game has a flag toggled to 0 that makes it use
				// GetTickCount(), but if set to 1 it uses QueryPerformanceCounter(), which is much
				// more precise and works better at high FPS. They likely disabled it because
				// QueryPerformanceCounter() would desync and drift on old AMD Athlon X2 CPUs.
				if (patternAddress != 0)
				{
					DEBUG_LOG("Signature for high precision timer found at 0x%X", patternAddress);
					void* patchAddress = reinterpret_cast<void*>(patternAddress + 7);

					DWORD oldProtect;
					if (VirtualProtect(patchAddress, 2, PAGE_EXECUTE_READWRITE, &oldProtect))
					{
						BYTE* pByte = static_cast<BYTE*>(patchAddress);
						pByte[0] = 0x90;
						pByte[1] = 0x90;

						VirtualProtect(patchAddress, 2, oldProtect, &oldProtect);
						DEBUG_LOG("Timer patched");
					}
				}
			}
		}
	}
}
