#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace Gameplay {

		namespace IntroCutscene {

			//Skips the Ishimura landing cutscene for both a fresh New Game and
			//New Game Plus (the two code paths seed the checkpoint differently).

			namespace NewGame {

				//A normal new game resolves the checkpoint by name. The "new game"
				//checkpoint name is game data (identical in every build), so locate it
				//by content and rewrite it to the "kellion" (landed on Ishimura) checkpoint
				//name. Both names are the same length, so the null terminator stays put.
				void Patch(HMODULE hExe)
				{
					uintptr_t checkpointNameAddress = Utils::FindString(hExe, "XCENTKOWSK_C8A99CD_622DBBB_v3");
					if (checkpointNameAddress != 0)
					{
						const char* landingSeenName = "XCENTKOWSK_C78C369_F71988A_v3";
						if (Utils::WriteBytes(checkpointNameAddress, landingSeenName, 29))
							LOG_INFO("[Patches/Gameplay/IntroCutscene]", "Skipped intro!");
						else
							LOG_WARN("[Patches/Gameplay/IntroCutscene]", "Failed to skip intro!");
					}
				}
			}

			namespace NewGamePlus {

				//NG+ seeds the 16-byte checkpoint record from object state at runtime
				//(a normal new game resolves the checkpoint by name instead), so a content
				//rewrite alone can't cover it. We hook the record-creating function with
				//MinHook and substitute the "kellion" (landed on Ishimura) checkpoint for the
				//"initial" before it's committed. The checkpoint hashes are derived from the
				//checkpoint names in the game's save data, so these constants are stable
				//across builds.
				typedef void(__cdecl* CreateCheckpointRecord_t)(void* arg1, void* arg2, uint32_t rec0, uint32_t rec1, uint32_t rec2, uint32_t rec3);
				CreateCheckpointRecord_t oCreateCheckpointRecord = nullptr;

				void __cdecl hkCreateCheckpointRecord(void* arg1, void* arg2, uint32_t rec0, uint32_t rec1, uint32_t rec2, uint32_t rec3)
				{
					static const uint32_t kCheckpointIntro[4] = { 0x4BC8A99C, 0xD622DBBB, 0x544E4543, 0x53574F4B };
					static const uint32_t kCheckpointIshimura[4] = { 0x4BC78C36, 0x9F71988A, 0x544E4543, 0x53574F4B };

					if (rec0 == kCheckpointIntro[0] && rec1 == kCheckpointIntro[1] &&
						rec2 == kCheckpointIntro[2] && rec3 == kCheckpointIntro[3])
					{
						rec0 = kCheckpointIshimura[0];
						rec1 = kCheckpointIshimura[1];
						rec2 = kCheckpointIshimura[2];
						rec3 = kCheckpointIshimura[3];
					}

					oCreateCheckpointRecord(arg1, arg2, rec0, rec1, rec2, rec3);
				}

				void Patch(HMODULE hExe)
				{
					const char* ngPlusSig = "83 EC 0C 56 57 E8 ?? ?? ?? ?? 64 A1 2C 00 00 00 8B 30";
					uintptr_t ngPlusAddress = Utils::FindPattern(hExe, ngPlusSig);
					if (ngPlusAddress != 0)
					{
						if (MH_CreateHook(reinterpret_cast<void*>(ngPlusAddress), &hkCreateCheckpointRecord,
							reinterpret_cast<LPVOID*>(&oCreateCheckpointRecord)) == MH_OK &&
							MH_EnableHook(reinterpret_cast<void*>(ngPlusAddress)) == MH_OK)
						{
							LOG_INFO("[Patches/Gameplay/IntroCutscene]", "NG+ landing cutscene skip hooked at 0x%X", ngPlusAddress);
						}
						else
						{
							LOG_WARN("[Patches/Gameplay/IntroCutscene]", "Failed to hook NG+ landing cutscene skip");
						}
					}
				}
			}

			void Apply(HMODULE hExe)
			{
				NewGame::Patch(hExe);
				NewGamePlus::Patch(hExe);
			}
		}
	}
}
