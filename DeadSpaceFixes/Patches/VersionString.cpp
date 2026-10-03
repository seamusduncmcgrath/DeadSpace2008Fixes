#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace UI {

		namespace VersionString {
			void Apply(HMODULE hExe)
			{
				const char* versionSignature = "68 ? ? ? ? 6A 64 68 ? ? ? ? E8 ? ? ? ? 83 C4 1C";
				uintptr_t versionAddress = Utils::FindPattern(hExe, versionSignature);
				
				if (versionAddress != 0)
				{
					//versionAddress + 1 points to the address of the "v%d.%d.%d.%d" string
					char** ppFormatString = reinterpret_cast<char**>(versionAddress + 1);
					
#ifdef _DEBUG
					const char* customVersion = "DeadSpaceFixes Installed! (Debug)";
#else
					const char* customVersion = "DeadSpaceFixes Installed!";
#endif

					DWORD oldProtect;
					if (VirtualProtect(ppFormatString, sizeof(char*), PAGE_EXECUTE_READWRITE, &oldProtect))
					{
						*ppFormatString = const_cast<char*>(customVersion);
						
						VirtualProtect(ppFormatString, sizeof(char*), oldProtect, &oldProtect);
						LOG_INFO("[Patches/UI/VersionString]", "Successfully injected custom format string pointer.");
					}
				}
			}
		}
	}
}