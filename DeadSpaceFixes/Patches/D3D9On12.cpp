#include "common.h"
#include "utils.h"

#include "Config.h"

namespace Features
{
	namespace Graphics
	{
		namespace D3D9On12
		{
			static Create9_t       Create9;
			static Create9Ex_t     Create9Ex;
			static Create9On12_t   Create9On12;
			static Create9On12Ex_t Create9On12Ex;

            constexpr const char* kTag = "[Features/Graphics/D3D9On12]";

            static void* WINAPI Hook_Create9(UINT sdk) 
            {
                if (Create9On12) 
                {
                    D3D9ON12_ARGS_ args = {};
                    args.Enable9On12 = TRUE;

                    void* d3d = Create9On12(sdk, &args, 1);
                    LOG_INFO(kTag,"Direct3DCreate9(%u) -> Direct3DCreate9On12 = %p", sdk, d3d);

                    if (d3d) return d3d;
                    LOG_INFO(kTag, "On12 creation failed, falling back to native D3D9");
                }
                return Create9(sdk);
            }

            static bool PatchSlot(void** slot, void* hook) {
                DWORD old;
                if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
                *slot = hook;
                VirtualProtect(slot, sizeof(void*), old, &old);
                return true;
            }

            static int PatchImports(HMODULE module, void* target, void* hook) {
                if (!target) return 0;
                BYTE* base = (BYTE*)module;
                IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
                if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
                IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
                if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
                IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
                if (!dir.VirtualAddress) return 0;

                int patched = 0;
                for (IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; ++imp) 
                {
                    if (_stricmp((const char*)(base + imp->Name), "d3d9.dll") != 0) continue;
                    for (void** slot = (void**)(base + imp->FirstThunk); *slot; ++slot)
                        if (*slot == target && PatchSlot(slot, hook)) ++patched;
                }
                return patched;
            }

			void Apply(HMODULE hExe)
			{
                if (!Config::Features::D3D9On12)
                    return;

                HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
                if (!d3d9) d3d9 = LoadLibraryA("d3d9.dll");

                Create9 = (Create9_t)GetProcAddress(d3d9, "Direct3DCreate9");
                Create9On12 = (Create9On12_t)GetProcAddress(d3d9, "Direct3DCreate9On12");

                PatchImports(hExe, (void*)Create9, (void*)Hook_Create9);
			}
		}
	}
}