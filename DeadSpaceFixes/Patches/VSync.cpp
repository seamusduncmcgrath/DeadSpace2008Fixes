#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Graphics {

		namespace VSync {

			void ApplyStartup()
			{
				HMODULE hExe = GetModuleHandleA(nullptr);
				if (!hExe) return;

				//signature for MOV EBP, 2
				const char* vsyncStartupSig = "64 A1 ? ? ? ? 8B 08 8B 49 0C BD 02 00 00 00 33 FF";
				uintptr_t vsyncStartupAddress = Utils::FindPattern(hExe, vsyncStartupSig);

				if (vsyncStartupAddress != 0)
				{
					//+12 bytes to hit the '02'
					void* patchAddress = reinterpret_cast<void*>(vsyncStartupAddress + 12);

					DWORD oldProtect;
					if (VirtualProtect(patchAddress, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
					{
						//0x00 uncaps it, 0x01 sets it to 60, 0x02 sets it to 30 (which is what the game normally uses)
						*static_cast<BYTE*>(patchAddress) = 0x00;
						VirtualProtect(patchAddress, 1, oldProtect, &oldProtect);
					}
				}
			}

			void Apply(HMODULE hExe)
			{
				//we change the mov edx, 2 to mov edx, 0 to disable the FPS cap since theres no actual VSync in windowed
				const char* vsyncMenuSig = "BA 02 00 00 00 EB ? 33 D2 89 15";
				uintptr_t vsyncmenuAddress = Utils::FindPattern(hExe, vsyncMenuSig) + 1;

				if (vsyncmenuAddress != 0)
				{
					const BYTE disableCap = 0x00;
					if (Utils::WriteBytes(vsyncmenuAddress, &disableCap, sizeof(disableCap)))
						LOG_INFO("[Fixes/Graphics/VSync]", "Patched software vsync mode!");
				}

				// patch D3D9 PresentationInterval for fullscreen VSync
				// the game requests D3DPRESENT_INTERVAL_TWO in fullscreen,
				// which caps the framerate to half your monitor's refresh rate (e.g. 30fps on 60hz).
				// we change this to D3DPRESENT_INTERVAL_ONE so it syncs to the refresh rate instead.
				const char* d3dIntervalSig = "C7 47 34 02 00 00 00 EB ? 0F B6 D0 23 D1";
				uintptr_t d3dIntervalAddress = Utils::FindPattern(hExe, d3dIntervalSig);

				if (d3dIntervalAddress != 0)
				{
					const BYTE intervalOne = 0x01;
					// +3 bytes to hit the '02' in C7 47 34 02
					if (Utils::WriteBytes(d3dIntervalAddress + 3, &intervalOne, sizeof(intervalOne)))
						LOG_INFO("[Fixes/Graphics/VSync]", "Patched D3D PresentationInterval for Fullscreen VSync!");
				}

				// Patch D3D9 PresentationInterval for Windowed VSync
				// inn windowed mode the game forces D3DPRESENT_INTERVAL_IMMEDIATE no matter of the VSync setting.
				// Because we removed the 30 FPS cap above, windowed mode runs completely uncapped even with VSync on.
				// We allocate a small code cave to dynamically check the VSync menu setting (stored in `bl`)
				// and set the interval to 1 (VSync ON) or 0x80000000 (VSync OFF) accordingly.
				// Yes I know a hook would be better but this works
				const char* d3dWindowedIntervalSig = "C7 47 34 00 00 00 80 89 6F 30 64 A1 2C 00 00 00";
				uintptr_t d3dWindowedIntervalAddress = Utils::FindPattern(hExe, d3dWindowedIntervalSig);

				if (d3dWindowedIntervalAddress != 0)
				{
					void* cave = VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
					if (cave)
					{
						DWORD returnAddress = d3dWindowedIntervalAddress + 10;

						BYTE shellcode[] = {
							0x89, 0x6F, 0x30,                               // mov [edi+30h], ebp
							0x80, 0xBC, 0x24, 0x9C, 0x01, 0x00, 0x00, 0x00, // cmp byte ptr [esp+19Ch], 0
							0x74, 0x0C,                                     // jz vsync_off
							0xC7, 0x47, 0x34, 0x01, 0x00, 0x00, 0x00,       // mov dword ptr [edi+34h], 1
							0xE9, 0x00, 0x00, 0x00, 0x00,                   // jmp return_addr
							// vsync_off:
							0xC7, 0x47, 0x34, 0x00, 0x00, 0x00, 0x80,       // mov dword ptr [edi+34h], 80000000h
							0xE9, 0x00, 0x00, 0x00, 0x00                    // jmp return_addr
						};

						*(DWORD*)(shellcode + 21) = returnAddress - ((DWORD)cave + 25);
						*(DWORD*)(shellcode + 33) = returnAddress - ((DWORD)cave + 37);

						memcpy(cave, shellcode, sizeof(shellcode));

						//write 5 byte JMP and NOP out the rest of the 10 bytes
						BYTE jmpPatch[10] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90 };
						*(DWORD*)(jmpPatch + 1) = (DWORD)cave - (d3dWindowedIntervalAddress + 5);

						if (Utils::WriteBytes(d3dWindowedIntervalAddress, jmpPatch, sizeof(jmpPatch)))
							LOG_INFO("[Fixes/Graphics/VSync]", "Patched D3D PresentationInterval for Windowed VSync!");
					}
				}
			}
		}
	}
}
