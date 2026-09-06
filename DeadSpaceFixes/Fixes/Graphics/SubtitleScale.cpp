#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Graphics {

		namespace SubtitleScale {

			uintptr_t subtitleHookReturn = 0;
			void* oSubtitleSettings = nullptr;
			//feel like 0.8 is a better baseline, subtitles clip out less then
			float subtitleScale = 1.0f;

			__declspec(naked) void hkSubtitleScale() //note to self comment well
			{
				__asm
				{
					pushfd

					cmp edi, 0 //is it the inventory or other menus?
					je original_code //if yes skip to original code

					//if it's a subtitle replace scale param
					push eax
					mov eax, dword ptr[subtitleScale]
					mov dword ptr[ebp + 0x18], eax
					pop eax

					original_code :
					popfd //restore cpu flags

						mulss xmm0, dword ptr[ebp + 0x18] //original instruction

						jmp[subtitleHookReturn] //jump back to engine
				}
			}

#pragma pack(push, 1)
			struct SubtitleSettings {
				float x;         // 0x00 (EDI)
				float y;         // 0x04 (EDI + 0x4)
				float boundingBoxX;    // 0x08 (EDI + 0x8)
				float boundingBoxY;    // 0x0C (EDI + 0xC)
				uint32_t unk10;  // 0x10
				uint32_t unk14;  // 0x14
				float fontScale;     // 0x18
				uint32_t pad1C;  // 0x1C
				uint32_t color1; // 0x20
				uint32_t color2; // 0x24
				uint8_t  pad28;  // 0x28
				uint8_t  flag29; // 0x29
			};
#pragma pack(pop)

			void __stdcall LogSubtitleFunc(void* eax, int ecx, uint32_t stackParam) {
				SubtitleSettings** pData = reinterpret_cast<SubtitleSettings**>(eax);
				if (!pData || !*pData) return;

				SubtitleSettings* data = *pData;

				int screenWidth = GetSystemMetrics(SM_CXSCREEN);
				int screenHeight = GetSystemMetrics(SM_CYSCREEN);

				float subtitleMultiplier = static_cast<float>(screenHeight) / 720.0f;
				float aspectFix = (static_cast<float>(screenWidth) / screenHeight) / (16.0f / 9.0f);

				//define the games origional values, easier to just do this rather than get em at runtime
				const float origX = 0.20f;
				const float origY = 0.25f;
				const float origW = 0.60f;
				const float origH = 0.10f;
				const float origScale = 0.60f;

				float newW = origW * subtitleMultiplier * aspectFix;
				float newH = origH * subtitleMultiplier;
				float newScale = origScale * subtitleMultiplier;

				//calculate the original center points
				//center = start position + (size / 2)
				float centerX = origX + (origW / 2.0f); // Always 0.5 (perfect horizontal center)
				float centerY = origY + (origH / 2.0f); // Always 0.3

				//shift the X and Y start positions so the expanded bounding box stays centered
				data->x = centerX - (newW / 2.0f);
				data->y = centerY - (newH / 2.0f);

				data->boundingBoxX = newW;
				data->boundingBoxY = newH;
				data->fontScale = newScale;
			}

			//naked hook cause compiler bs
			__declspec(naked) void hkSubtitleSettings() {
				__asm {
					pushad
					pushfd

					mov edx, dword ptr[esp + 40]

					push edx
					push ecx
					push eax

					call LogSubtitleFunc

					popfd
					popad

					jmp dword ptr[oSubtitleSettings]
				}
			}

			void Apply(HMODULE hExe)
			{
				const char* subtitleSettingsSignature = "55 8B EC 83 E4 F8 83 EC 14 53 56 8B F1 57 8B 38 8B 4F";
				uintptr_t subtitleAddres = Utils::FindPattern(hExe, subtitleSettingsSignature);

				if (subtitleAddres != 0)
				{
					LOG_INFO("[Fixes/Graphics/SubtitleScale]", "Hooked subtitle settings at 0x%X", subtitleAddres);
					MH_CreateHook(reinterpret_cast<void*>(subtitleAddres), &hkSubtitleSettings, &oSubtitleSettings);
					MH_EnableHook(reinterpret_cast<void*>(subtitleAddres));
				}
			}
		}
	}
}
