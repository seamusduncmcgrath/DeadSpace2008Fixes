#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace UI {

		namespace VersionString {
			//yet another fix for the version string
			const std::size_t kVersionBufferSize = 100;

			void Apply(HMODULE hExe)
			{
				const char* versionSignature = "68 ? ? ? ? 6A 64 68 ? ? ? ? E8 ? ? ? ? 83 C4 1C";
				uintptr_t versionAddress = Utils::FindPattern(hExe, versionSignature);

				if (versionAddress != 0)
				{
					//versionAddress + 1 points to the address of the "v%d.%d.%d.%d" string,
					//versionAddress + 8 to the address of the buffer it gets printed into
					char** ppFormatString = reinterpret_cast<char**>(versionAddress + 1);
					char* pVersionBuffer = *reinterpret_cast<char**>(versionAddress + 8);

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
					}

					strncpy(pVersionBuffer, customVersion, kVersionBufferSize - 1);
					pVersionBuffer[kVersionBufferSize - 1] = '\0';
				}
			}
		}
	}
}