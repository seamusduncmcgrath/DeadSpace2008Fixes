#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace UI {

		namespace LoadingScreen {

			//The loading screen timeline driver compares the elapsed
			//time against SimGroup+0xB0 (pre-first-tip hold, 15.0s) and SimGroup+0xB4
			//(per-tip cycle, 1.0s). The comiss instructions are retargeted to these floats
			//so the post-load hold is ~3s instead of ~31s while tips still show. DLL data
			//so the address is stable and a config reload can rewrite the values.
			float g_LoadingTipPreHold = 2.0f;
			float g_LoadingTipCycleHold = 1.0f;

			void Apply(HMODULE hExe)
			{
				//credits to MarkerPatch for idea and name

				//Shorten the artificial post-load hold on the
				//loading screen instead of removing the tip text. After the level
				//finishes loading, the timeline driver (FUN_00617620) waits
				//SimGroup+0xB0 (15.0s) before showing the first tip, then cycles one
				//tip every SimGroup+0xB4 (1.0s) before hiding the screen. Retarget
				//both comiss hold comparisons (0x00617694 / 0x006176BB) to our own
				//floats so tips appear quickly and the screen cuts ~3s after
				//load-done. Both sites are 7-byte comiss xmm0,[m32] instructions, so
				//the replacement (0F 2F 05 <disp32>) is the same length.
				//Reverse engineered and verified by AI.
				const char* loadingPreHoldSig = "F3 0F 10 46 14 0F 2F 80 B0 00 00 00 76 41 0F 57 C0";
				uintptr_t loadingPreHold = Utils::FindPattern(hExe, loadingPreHoldSig);
				if (loadingPreHold != 0) {
					DEBUG_LOG("Found loading screen pre-tip hold at 0x%X", loadingPreHold + 5);
					BYTE patch[7] = { 0x0F, 0x2F, 0x05 };
					*reinterpret_cast<uint32_t*>(patch + 3) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_LoadingTipPreHold));
					if (Utils::WriteBytes(loadingPreHold + 5, patch, sizeof(patch)))
						DEBUG_LOG("Patched loading screen pre-tip hold to %.1fs", g_LoadingTipPreHold);
				}

				//The per-tip hold. Retarget to our
				//own float the same way so every tip after the first also cuts quickly.
				const char* loadingCycleSig = "8B 56 28 0F 2F 82 B4 00 00 00 76 1A C7 46 24 04";
				uintptr_t loadingCycle = Utils::FindPattern(hExe, loadingCycleSig);
				if (loadingCycle != 0) {
					DEBUG_LOG("Found loading screen tip cycle hold at 0x%X", loadingCycle + 3);
					BYTE patch[7] = { 0x0F, 0x2F, 0x05 };
					*reinterpret_cast<uint32_t*>(patch + 3) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_LoadingTipCycleHold));
					if (Utils::WriteBytes(loadingCycle + 3, patch, sizeof(patch)))
						DEBUG_LOG("Patched loading screen tip cycle hold to %.1fs", g_LoadingTipCycleHold);
				}
			}
		}
	}
}
