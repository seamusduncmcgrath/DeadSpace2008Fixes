#include "common.h"
#include "Utils.h"

#include <cmath>

namespace Input {

	namespace MenuCamera {

		static constexpr const char* kTag = "[Features/Input/MenuCamera]";

		//how hard the cursor at the edge of the screen counts as pushing the stick, 1.0 is all the way
		const float kCursorScale = 1.0f;

		void* oMenuCameraUpdate = nullptr; //takes the main menu in EAX so we can only call it from asm

		BYTE** g_ppUiInput = nullptr; //the right stick is the two floats at +0x70, after the game's deadzone
		BYTE** g_ppCursor = nullptr;
		BYTE* g_pInvertX = nullptr;
		BYTE* g_pInvertY = nullptr;

		bool g_UsingMouse = false;
		bool g_StickOverridden = false;
		float g_SavedStick[2] = {};

		void __stdcall BeforeMenuCamera()
		{
			g_StickOverridden = false;

			BYTE* pUiInput = *g_ppUiInput;
			BYTE* pCursor = *g_ppCursor;
			if (!pUiInput || !pCursor)
				return;

			float* pStick = reinterpret_cast<float*>(pUiInput + 0x70);

			if (pStick[0] != 0.0f || pStick[1] != 0.0f)
			{
				g_UsingMouse = false;
				return;
			}
			if (pCursor[0x42])
				g_UsingMouse = true;

			//the same checks the game makes before it moves the cursor: enabled (+0x40) and has a sprite (+0x54)
			if (!g_UsingMouse || !pCursor[0x40] || !*reinterpret_cast<void**>(pCursor + 0x54))
				return;

			float x = *reinterpret_cast<float*>(pCursor + 0x10) * kCursorScale;
			float y = -*reinterpret_cast<float*>(pCursor + 0x14) * kCursorScale;

			//keep it inside the stick's circle, the corners would push further than a real stick can
			float lengthSq = x * x + y * y;
			if (lengthSq > 1.0f)
			{
				float scale = 1.0f / sqrtf(lengthSq);
				x *= scale;
				y *= scale;
			}

			//the game flips the stick for Invert X/Y, flip ours first so the view always leans towards the cursor
			if (*g_pInvertX)
				x = -x;
			if (*g_pInvertY)
				y = -y;

			g_SavedStick[0] = pStick[0];
			g_SavedStick[1] = pStick[1];
			pStick[0] = x;
			pStick[1] = y;
			g_StickOverridden = true;
		}

		void __stdcall AfterMenuCamera()
		{
			if (!g_StickOverridden)
				return;

			float* pStick = reinterpret_cast<float*>(*g_ppUiInput + 0x70);
			pStick[0] = g_SavedStick[0];
			pStick[1] = g_SavedStick[1];
		}

		__declspec(naked) void hkMenuCameraUpdate()
		{
			__asm
			{
				pushad
				call BeforeMenuCamera
				popad

				call dword ptr [oMenuCameraUpdate]

				pushad
				call AfterMenuCamera
				popad
				ret
			}
		}

		void Apply(HMODULE hExe)
		{
			const char* menuCameraSignature = "8B 48 50 F3 0F 10 81 8C 02 00 00 0F 57 E4 83 EC 0C 56 8B 35 ? ? ? ? 8B 4E 38 85 C9 57 74 1D";
			uintptr_t menuCameraAddress = Utils::FindPattern(hExe, menuCameraSignature);
			if (menuCameraAddress == 0)
			{
				LOG_WARN(kTag, "Couldn't find the menu camera");
				return;
			}

			//further in it does mov edx, [uiInput], then lea ecx, [edx+70h] for the stick, then
			//cmp byte ptr [invertX], 0 and cmp byte ptr [invertY], 0. Pull the addresses out of those.
			BYTE* pUiInputLoad = reinterpret_cast<BYTE*>(menuCameraAddress + 0x88);
			BYTE* pStickLea = reinterpret_cast<BYTE*>(menuCameraAddress + 0x102);
			BYTE* pInvertXCheck = reinterpret_cast<BYTE*>(menuCameraAddress + 0x10D);
			BYTE* pInvertYCheck = reinterpret_cast<BYTE*>(menuCameraAddress + 0x135);

			if (pUiInputLoad[0] != 0x8B || pUiInputLoad[1] != 0x15
				|| pStickLea[0] != 0x8D || pStickLea[1] != 0x4A || pStickLea[2] != 0x70
				|| pInvertXCheck[0] != 0x80 || pInvertXCheck[1] != 0x3D
				|| pInvertYCheck[0] != 0x80 || pInvertYCheck[1] != 0x3D)
			{
				LOG_WARN(kTag, "The menu camera isn't laid out how we expect, not hooking it");
				return;
			}

			//the cursor's constructor clears its fields then stores itself: mov [cursor], ecx
			const char* cursorSignature = "89 5F 54 88 5F 40 88 5F 42 88 47 41 8B CF 89 5F 44 89 5F 48 89 5F 4C 89 5F 50 39 1D ? ? ? ? 89 0D ? ? ? ?";
			uintptr_t cursorAddress = Utils::FindPattern(hExe, cursorSignature);
			if (cursorAddress == 0)
			{
				LOG_WARN(kTag, "Couldn't find the menu cursor");
				return;
			}

			g_ppUiInput = *reinterpret_cast<BYTE***>(pUiInputLoad + 2);
			g_pInvertX = *reinterpret_cast<BYTE**>(pInvertXCheck + 2);
			g_pInvertY = *reinterpret_cast<BYTE**>(pInvertYCheck + 2);
			g_ppCursor = *reinterpret_cast<BYTE***>(cursorAddress + 0x22);

			void* pMenuCameraTarget = reinterpret_cast<void*>(menuCameraAddress);

			if (MH_CreateHook(pMenuCameraTarget, &hkMenuCameraUpdate, &oMenuCameraUpdate) == MH_OK)
			{
				MH_EnableHook(pMenuCameraTarget);
				LOG_INFO(kTag, "Hooked the menu camera at 0x%X", menuCameraAddress);
			}
		}
	}
}
