#include "common.h"
#include "Utils.h"

namespace Input {

	namespace MouseLook {

		static constexpr const char* kTag = "[Fixes/Input/MouseLook]";

		const float kAimSensitivityScale = 0.15f;

		const float kRadToDeg = 57.29578f;

		typedef int(__stdcall* CameraLook_t)(BYTE* pCamera, float dt);

		CameraLook_t oCameraLook = nullptr;
		void* oAimLook = nullptr;

		BYTE** g_ppInputManager = nullptr;
		BYTE* g_pInvertX = nullptr; //per player, and backwards: 0 means the axis gets flipped
		BYTE* g_pInvertY = nullptr;

		//the game multiplies the camera's yaw rate by a global (1.15) pitch is left alone
		float g_DefaultCameraYawScale = 1.15f;
		float* g_pCameraYawScale = &g_DefaultCameraYawScale;

		//how far the walking camera turns per unit of mouse "stick" in radians, read off the
		//camera so aiming can use the same. Starts at the game's defaults (3.5 degrees).
		float g_WalkYawRate = 0.0610865f * 1.15f;
		float g_WalkPitchRate = 0.0610865f;
		DWORD g_LastAimTick = 0;

		//The mouse gets a pad slot of its own. Returns its right stick X/Y, or null if there
		//isn't an active mouse. Pad states are 0x44 bytes each starting at inputManager+0x28,
		//with the right stick floats at +0x8/+0xC.
		float* GetMouseStick()
		{
			BYTE* pInputManager = *g_ppInputManager;
			if (!pInputManager)
				return nullptr;

			BYTE slot = pInputManager[0x560];
			if (slot >= 16 || !pInputManager[0x529 + slot])
				return nullptr;

			return reinterpret_cast<float*>(pInputManager + 0x28 + slot * 0x44 + 0x8);
		}

		int __stdcall hkCameraLook(BYTE* pCamera, float dt)
		{
			//the game multiplies the turn by this (dt * 59.94 clamped to 0..1), which is
			//why the camera gets slower above 60 FPS
			float dtNorm = *reinterpret_cast<float*>(pCamera + 0x60);

			//remember the walking turn rates for AimLook. The camera's settings can change
			//while aiming, so only trust them when we haven't aimed for a moment
			float yawRate = *reinterpret_cast<float*>(pCamera + 0x12C) * *g_pCameraYawScale;
			float pitchRate = *reinterpret_cast<float*>(pCamera + 0x134);
			if (GetTickCount() - g_LastAimTick > 250 && yawRate > 0.0f && pitchRate > 0.0f
				&& (yawRate != g_WalkYawRate || pitchRate != g_WalkPitchRate))
			{
				g_WalkYawRate = yawRate;
				g_WalkPitchRate = pitchRate;
				LOG_DEBUG(kTag, "Walking turn rate is now (%.3f, %.3f) degrees per stick", yawRate * kRadToDeg, pitchRate * kRadToDeg);
			}

			float* pMouseStick = GetMouseStick();
			if (!pMouseStick || dtNorm <= 0.001f)
				return oCameraLook(pCamera, dt);

			//cancel out dtNorm so one mouse count is always the same amount of turn, then
			//put the stick back for whatever reads it after us this frame
			float mouseX = pMouseStick[0];
			float mouseY = pMouseStick[1];
			pMouseStick[0] = mouseX / dtNorm;
			pMouseStick[1] = mouseY / dtNorm;

			int result = oCameraLook(pCamera, dt);

			pMouseStick[0] = mouseX;
			pMouseStick[1] = mouseY;

			return result;
		}

		void __stdcall AimLookPost(BYTE* pState, float* pYawDeg, float* pPitchDeg, float /*dt*/, float scaleX, float scaleY)
		{
			g_LastAimTick = GetTickCount();

			float* pMouseStick = GetMouseStick();
			if (!pMouseStick || (pMouseStick[0] == 0.0f && pMouseStick[1] == 0.0f))
				return;

			BYTE* pPlayer = *reinterpret_cast<BYTE**>(pState + 0x50);
			int playerIndex = *reinterpret_cast<int*>(pPlayer + 0x580);

			float x = pMouseStick[0] * scaleX;
			float y = -pMouseStick[1] * scaleY;
			if (!g_pInvertX[playerIndex]) x = -x;
			if (!g_pInvertY[playerIndex]) y = -y;

			//straight from the mouse at the walking camera's rates, skipping the game's
			//stick deadzone, acceleration and dt
			*pYawDeg = x * g_WalkYawRate * kRadToDeg * kAimSensitivityScale;
			*pPitchDeg = y * g_WalkPitchRate * kRadToDeg * kAimSensitivityScale;
		}

		//I feel like i use a silly amount of naked asm hooks
		__declspec(naked) void hkAimLook()
		{
			__asm
			{
				push ebp
				mov ebp, esp

				push dword ptr [ebp + 18h]
				push dword ptr [ebp + 14h]
				push dword ptr [ebp + 10h]
				push dword ptr [ebp + 0Ch]
				push dword ptr [ebp + 8]
				call dword ptr [oAimLook]
				push eax //keep the original's return value

				push dword ptr [ebp + 18h]
				push dword ptr [ebp + 14h]
				push dword ptr [ebp + 10h]
				push dword ptr [ebp + 0Ch]
				push dword ptr [ebp + 8]
				push esi
				call AimLookPost

				pop eax
				pop ebp
				ret 14h
			}
		}

		void Apply(HMODULE hExe)
		{
			uintptr_t inputManagerRef = Utils::FindPattern(hExe, "51 8B 0D ? ? ? ? 8A 81 60 05 00 00");
			if (inputManagerRef == 0)
			{
				LOG_WARN(kTag, "Couldn't find the input manager, not hooking anything");
				return;
			}
			g_ppInputManager = *reinterpret_cast<BYTE***>(inputManagerRef + 3);

			const char* cameraLookSignature = "55 8B EC 83 E4 F0 81 EC 14 01 00 00 53 8B 5D ? 8B 43 ? 85 C0 56 57 0F 84";
			uintptr_t cameraLookAddress = Utils::FindPattern(hExe, cameraLookSignature);

			if (cameraLookAddress != 0)
			{
				//further in it does movss xmm1, [ebx+12Ch] ... mulss xmm1, [yawScale], the
				//address of the yaw scale is the last 4 bytes of that
				const char* yawScaleSignature = "F3 0F 10 8B 2C 01 00 00 F3 0F 59 E3 F3 0F 59 CB F3 0F 59 0D";
				uintptr_t yawScaleRef = Utils::FindPattern(hExe, yawScaleSignature, cameraLookAddress, cameraLookAddress + 0x900);
				if (yawScaleRef != 0)
					g_pCameraYawScale = *reinterpret_cast<float**>(yawScaleRef + 20);
				else
					LOG_WARN(kTag, "Couldn't find the camera yaw scale, assuming 1.15");

				void* pCameraLookTarget = reinterpret_cast<void*>(cameraLookAddress);

				if (MH_CreateHook(pCameraLookTarget, &hkCameraLook, reinterpret_cast<LPVOID*>(&oCameraLook)) == MH_OK)
				{
					MH_EnableHook(pCameraLookTarget);
					LOG_INFO(kTag, "Hooked CameraLook at 0x%X", cameraLookAddress);
				}
			}
			else
			{
				LOG_WARN(kTag, "Couldn't find CameraLook");
			}

			const char* aimLookSignature = "55 8B EC 83 E4 F8 83 EC 08 53 8B 1D ? ? ? ? 57 8B 7E 50";
			uintptr_t aimLookAddress = Utils::FindPattern(hExe, aimLookSignature);

			if (aimLookAddress != 0)
			{
				//the invert X/Y flags are checked a little way into the function with
				//cmp byte ptr [edi+addr], 0 (80 BF addr 00), pull the addresses out of those
				BYTE* pInvertXCheck = reinterpret_cast<BYTE*>(aimLookAddress + 0xA5);
				BYTE* pInvertYCheck = reinterpret_cast<BYTE*>(aimLookAddress + 0xC3);

				g_pInvertX = *reinterpret_cast<BYTE**>(pInvertXCheck + 2);
				g_pInvertY = *reinterpret_cast<BYTE**>(pInvertYCheck + 2);

				void* pAimLookTarget = reinterpret_cast<void*>(aimLookAddress);

				if (MH_CreateHook(pAimLookTarget, &hkAimLook, &oAimLook) == MH_OK)
				{
					MH_EnableHook(pAimLookTarget);
					LOG_INFO(kTag, "Hooked AimLook at 0x%X", aimLookAddress);
				}
			}
			else
			{
				LOG_WARN(kTag, "Couldn't find AimLook");
			}
		}
	}
}
