#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Save {

		namespace SafeStringHandling {

			SaveStringCopy_t oSaveStringCopy = nullptr;

			errno_t hkSaveStringCopy(wchar_t* dest, wchar_t* src) //just making the save file string handling a bit better
			{
				if (src != nullptr)
				{
					//instead of _wcscpy_s which aborts the game on overflow we use wcsncpy to truncate the path to 127 chars
					//doesn't fix really anything but is safer
					wcsncpy(dest, src, 127);
					dest[127] = L'\0'; //ensure string is null terminated
					return 0;
				}
				//theres a bug with the game where it clears 128 bytes rather than 128 wide characters (256 bytes), this should fix it
				memset(dest, 0, 128 * sizeof(wchar_t));
				return -1;
			}

			void Apply(HMODULE hExe)
			{
				const char* saveStringSignature = "8B 44 24 08 85 C0 74 14 50 8B 44 24 08 68 80 00"; //credit to marker patch for this
				uintptr_t saveStringAddress = Utils::FindPattern(hExe, saveStringSignature);
				if (saveStringAddress != 0)
				{
					void* pSaveCopyTarget = reinterpret_cast<void*>(saveStringAddress);
					DEBUG_LOG("Found save string handling at 0x%p", pSaveCopyTarget);

					if (MH_CreateHook(pSaveCopyTarget, &hkSaveStringCopy, reinterpret_cast<LPVOID*>(&oSaveStringCopy)) == MH_OK)
					{
						MH_EnableHook(pSaveCopyTarget);
						DEBUG_LOG("Save string handling hooked, should be safer");
					}
				}
			}
		}
	}
}
