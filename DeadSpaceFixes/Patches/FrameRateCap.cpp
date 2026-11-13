#include "common.h"
#include "Utils.h"

namespace Features {

	namespace Graphics {

		namespace FrameRateCap {

			//Caps the framerate to keep physics/scripts stable at high FPS. Optional,
			//enabled via the CaveFPS config option.

			LARGE_INTEGER g_TimerFrequency;
			LARGE_INTEGER g_LastFrameTime;
			Present_t oPresent = nullptr;

			//todo: set arbitrary FPS value via config
			int g_TargetFPS = 60;

			HRESULT APIENTRY hkPresent(LPDIRECT3DDEVICE9 pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
			{
				if (g_TargetFPS > 0)
				{
					LARGE_INTEGER currentTime;
					LONGLONG targetTicksPerFrame = g_TimerFrequency.QuadPart / g_TargetFPS;

					while (true)
					{
						QueryPerformanceCounter(&currentTime);

						LONGLONG elapsed = currentTime.QuadPart - g_LastFrameTime.QuadPart;
						if (elapsed >= targetTicksPerFrame)
							break;

						//if were still far away sleep a bit
						if (targetTicksPerFrame - elapsed > g_TimerFrequency.QuadPart / 1000) //>1ms
						{
							Sleep(1);
						}
						else
						{
							YieldProcessor(); //proper fine grain wait, never knew about this till today lol
						}
					}

					g_LastFrameTime.QuadPart += targetTicksPerFrame;

					if (currentTime.QuadPart - g_LastFrameTime.QuadPart > targetTicksPerFrame)
					{
						g_LastFrameTime = currentTime;
					}
				}
				return oPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
			}

			void Apply(IDirect3DDevice9* pDummyDevice)
			{
				QueryPerformanceFrequency(&g_TimerFrequency);
				QueryPerformanceCounter(&g_LastFrameTime);

				void** pVTable = *reinterpret_cast<void***>(pDummyDevice);
				void* pPresentTarget = pVTable[17];

				MH_CreateHook(pPresentTarget, &hkPresent, reinterpret_cast<LPVOID*>(&oPresent));
				MH_EnableHook(pPresentTarget);
			}
		}
	}
}
