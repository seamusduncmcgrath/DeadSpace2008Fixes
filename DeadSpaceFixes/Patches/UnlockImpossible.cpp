#include "common.h"
#include "Utils.h"

namespace Patches
{
    namespace Gameplay 
    {
        namespace UnlockImpossible
        {
            const char* sig = "6A 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 85 C0 7E 16 6A 00 B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 04 A3";

            void Apply(HMODULE hExe)
            {
                uintptr_t patchAddr = Utils::FindPattern(hExe, sig);

                if (patchAddr)
                {
                    //patch dword_E88E28 to always be 1 in sub_62AB00
                    const uint8_t jmpPatch = 0xEB;
                    Utils::WriteBytes(patchAddr + 0x11, &jmpPatch, sizeof(jmpPatch));

                    const uint32_t immOne = 1;
                    Utils::WriteBytes(patchAddr + 0x2F, &immOne, sizeof(immOne));

                    //also set dword_E88E28 to 1 incase sub_62AB00 already ran
                    uintptr_t gamePlayedAddr = *reinterpret_cast<uintptr_t*>(patchAddr + 0x23);
                    Utils::WriteBytes(gamePlayedAddr, &immOne, sizeof(immOne));
                }
            }
        }
    }
}
