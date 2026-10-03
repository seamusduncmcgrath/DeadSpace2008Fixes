#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Graphics {

		namespace VSync {

			// Runs very early (from DllMain) because this patch breaks if applied later.
			// Uncaps the startup VSync so it doesn't lock to 30 FPS.
			void ApplyStartup()
			{
				HMODULE hExe = GetModuleHandleA(nullptr);
				if (!hExe) return;

				// The signature for MOV EBP, 2
				const char* vsyncStartupSig = "64 A1 ? ? ? ? 8B 08 8B 49 0C BD 02 00 00 00 33 FF";
				uintptr_t vsyncStartupAddress = Utils::FindPattern(hExe, vsyncStartupSig);

				if (vsyncStartupAddress != 0)
				{
					// +12 bytes to hit the '02'
					void* patchAddress = reinterpret_cast<void*>(vsyncStartupAddress + 12);

					DWORD oldProtect;
					if (VirtualProtect(patchAddress, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
					{
						//0x00 uncaps it, 0x01 sets it to 60, 0x02 sets it to 30 (which is what the game normally uses)
						*static_cast<BYTE*>(patchAddress) = 0x00;
						VirtualProtect(patchAddress, 1, oldProtect, &oldProtect);
					}
				}
			}

			void Apply(HMODULE hExe)
			{
				const char* vsyncMenuSig = "BA 02 00 00 00 EB ? 33 D2 89 15";
				uintptr_t vsyncmenuAddress = Utils::FindPattern(hExe, vsyncMenuSig) + 1;

				if (vsyncmenuAddress != 0)
				{
					const BYTE disableCap = 0x00;
					if (Utils::WriteBytes(vsyncmenuAddress, &disableCap, sizeof(disableCap)))
						LOG_INFO("[Fixes/Graphics/VSync]", "Patched vsync mode!");
				}
			}
		}
	}
}
