#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace Gameplay {

		namespace SkipVideos {

			//Makes newly-triggered in-game videos (lore / vidlog) cancellable with
			//F, like optional item videos and database replays already are.
			//
			//The shared video skip gate (sub_587210) only refuses to cancel a
			//video when a "is skipping allowed?" check fails. New lore videos set
			//one of two of these checks — the block byte +0x275, or the +0x4C bit
			//1 flag (different videos/triggers set different ones). NOPing those
			//two jne's makes the normal path always pass when F is pressed; the
			//force-skip re-check then only *grants* a second chance and can never
			//revoke it, so it and the locked check can stay untouched. The F-input
			//check is left alone, so videos are still only cancelled on F.
			void Apply(HMODULE hExe)
			{
				//Entry of sub_587210 (the shared video skip gate).
				const char* gateSig = "83 EC 0C 55 8B 6C 24 14 8B 8D 94 02 00 00 32 C0 85 C9 0F 84 ?? ?? ?? ??";
				uintptr_t gate = Utils::FindPattern(hExe, gateSig);
				if (gate == 0) {
					LOG_WARN("[Patches/Gameplay/SkipVideos]", "Video skip gate signature not found");
					return;
				}

				//gate+0x56: jne after "block byte +0x275 == 0".
				//gate+0x6B: jne after "+0x4C bit 1" test.
				BYTE nop[2] = { 0x90, 0x90 };
				if (Utils::WriteBytes(gate + 0x56, nop, 2) &&
				    Utils::WriteBytes(gate + 0x6B, nop, 2))
					LOG_INFO("[Patches/Gameplay/SkipVideos]", "New in-game videos are now skippable (press F to cancel)");
				else
					LOG_WARN("[Patches/Gameplay/SkipVideos]", "Failed to patch the video skip gate");
			}
		}
	}
}