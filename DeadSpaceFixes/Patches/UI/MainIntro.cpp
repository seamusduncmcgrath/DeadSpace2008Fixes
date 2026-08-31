#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace UI {

		namespace MainIntro {

			//Skips the boot intro ("logo" / "press start" screens) and goes straight to the main menu.
			void Apply(HMODULE hExe)
			{
				//Boot state writes: state 2 and state 3 both become state 8 (attract).
				const char* bootState2Sig = "C7 05 ?? ?? ?? ?? 02 00 00 00 5F 5E 5B 8B E5 5D C2 04 00";
				uintptr_t bootState2 = Utils::FindPattern(hExe, bootState2Sig);
				if (bootState2 != 0) {
					LOG_DEBUG("[Patches/UI/MainIntro]", "Found frontend boot state 2 write at 0x%X", bootState2);
					const BYTE attract = 0x08;
					if (Utils::WriteBytes(bootState2 + 6, &attract, sizeof(attract)))
						LOG_INFO("[Patches/UI/MainIntro]", "Patched boot state 2 to attract state");
				}

				const char* bootState3Sig = "C7 05 ?? ?? ?? ?? 03 00 00 00 5F 5E 5B 8B E5 5D C2 04 00";
				uintptr_t bootState3 = Utils::FindPattern(hExe, bootState3Sig);
				if (bootState3 != 0) {
					LOG_DEBUG("[Patches/UI/MainIntro]", "Found frontend boot state 3 write at 0x%X", bootState3);
					const BYTE attract = 0x08;
					if (Utils::WriteBytes(bootState3 + 6, &attract, sizeof(attract)))
						LOG_INFO("[Patches/UI/MainIntro]", "Patched boot state 3 to attract state");
				}

				//Attract state (case 9): jump from its entry straight to its exit sequence
				const char* attractSig = "8B ? 50 F3 0F 10 ? 54 0F 2F ? 84 02 00 00";
				uintptr_t attractEntry = Utils::FindPattern(hExe, attractSig);
				if (attractEntry != 0) {
					const char* attractExitSig = "A1 ? ? ? ? E8 ? ? ? ? 8B ? 50 0F 57 C0 83 C0 5C F3 0F 11 ? 54";
					uintptr_t attractExit = Utils::FindPattern(hExe, attractExitSig, attractEntry, attractEntry + 0x400);
					if (attractExit != 0) {
						LOG_DEBUG("[Patches/UI/MainIntro]", "Found attract state at 0x%X with exit at 0x%X", attractEntry, attractExit);

						BYTE patch[8];
						patch[0] = 0xE9; //JMP
						*reinterpret_cast<uintptr_t*>(patch + 1) = attractExit - attractEntry - 5; //relative offset
						patch[5] = 0x90; //NOP the rest of the overwritten instructions
						patch[6] = 0x90;
						patch[7] = 0x90;
						if (Utils::WriteBytes(attractEntry, patch, sizeof(patch)))
							LOG_INFO("[Patches/UI/MainIntro]", "Patched attract state to exit immediately");
					}
				}
			}
		}
	}
}
