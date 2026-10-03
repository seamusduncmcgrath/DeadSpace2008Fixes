#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Graphics {

		namespace SubtitleScale {

			uintptr_t subtitleHookReturn = 0;
			void* oSubtitleSettings = nullptr;

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

				// 1080p = 1.5x | 4K = 3.0x
				float subtitleMultiplier = static_cast<float>(screenHeight) / 720.0f;
				float newScale = 0.60f * subtitleMultiplier;

				float aspectFix = (static_cast<float>(screenWidth) / screenHeight) / (16.0f / 9.0f);

				float boxW = 0.90f / aspectFix; // should make ultrawide better? gotta test it
				float boxH = 0.15f;

				//centers the box on screen
				float posX = 0.5f - (boxW / 2.0f);

				float posY = 0.25f;

				//apply
				data->x = posX;
				data->y = posY;
				data->boundingBoxX = boxW;
				data->boundingBoxY = boxH;
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
