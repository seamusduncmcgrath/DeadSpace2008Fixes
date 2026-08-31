#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace UI {

		namespace VersionString {

			//Shows a custom version string on the main menu footer instead of the game's version number.
			void Apply(HMODULE hExe)
			{
				const char* versionSignature = "68 ? ? ? ? 6A 64 68 ? ? ? ? E8 ? ? ? ? 83 C4 1C";
				uintptr_t versionAddress = Utils::FindPattern(hExe, versionSignature);
				if (versionAddress != 0)
				{
					char* pVersionString = *reinterpret_cast<char**>(versionAddress + 8);
					DEBUG_LOG("Found version number at 0x%p", pVersionString);
					DEBUG_LOG("Game version is %s", pVersionString);

					const char* customVersion = "DeadSpaceFixes Installed!";

					DWORD oldProtect;
					if (VirtualProtect(pVersionString, 100, PAGE_EXECUTE_READWRITE, &oldProtect))
					{
						strcpy_s(pVersionString, 100, customVersion);
						VirtualProtect(pVersionString, 100, oldProtect, &oldProtect);
						DEBUG_LOG("Set custom game version string to %s", customVersion);
					}
				}
			}
		}
	}
}
