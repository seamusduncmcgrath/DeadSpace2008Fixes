#include "common.h"
#include "Utils.h"

#include <cmath>

namespace Fixes
{
	namespace Graphics
	{
		namespace FixFullscreenBrightness
		{
			const char* kTag = "[Fixes/Graphics/FixFullscreenBrightness]";
			SetGammaRamp_t oSetGammaRamp = nullptr;

			namespace
			{
				constexpr float kDefaultSliderExponent = 0.74f;

				//recovers e from the ramp the game built, averaged over a few midtones, the game truncates each entry so +0.5 recentres it
				/// i love having to do random bs to fix stuff that shouldn't have gotten past QA
				float RecoverExponent(const D3DGAMMARAMP& ramp)
				{
					const int samples[] = { 64, 128, 192 };
					float sum = 0.0f;
					for (int i : samples)
					{
						float x = i / 255.0f;
						float y = (ramp.green[i] + 0.5f) / 65535.0f;
						sum += logf(y) / logf(x);
					}
					return sum / static_cast<float>(sizeof(samples) / sizeof(samples[0]));
				}
			}

			void __stdcall hkSetGammaRamp(IDirect3DDevice9* pDevice, UINT iSwapChain, DWORD Flags, const D3DGAMMARAMP* pRamp)
			{
				if (pRamp == nullptr)
					return oSetGammaRamp(pDevice, iSwapChain, Flags, pRamp);

				float exponent = RecoverExponent(*pRamp);

				if (fabsf(exponent - 1.0f) < 0.01f)
					return oSetGammaRamp(pDevice, iSwapChain, Flags, pRamp);

				//rescale the exponent instead of the output, so the default slider position becomes around the same as windowed
				float newExponent = exponent / kDefaultSliderExponent;

				LOG_DEBUG(kTag, "SetGammaRamp exponent %.3f -> %.3f", exponent, newExponent);

				D3DGAMMARAMP ramp;
				for (int i = 0; i < 256; i++)
				{
					WORD value = static_cast<WORD>(powf(i / 255.0f, newExponent) * 65535.0f + 0.5f);
					ramp.red[i] = value;
					ramp.green[i] = value;
					ramp.blue[i] = value;
				}

				oSetGammaRamp(pDevice, iSwapChain, Flags, &ramp);
			}

			void Apply(IDirect3DDevice9* pDummyDevice)
			{
				void** pVTable = *reinterpret_cast<void***>(pDummyDevice);
				void* pSetGammaRampTarget = pVTable[21];

				if (MH_CreateHook(pSetGammaRampTarget, &hkSetGammaRamp, reinterpret_cast<LPVOID*>(&oSetGammaRamp)) != MH_OK ||
					MH_EnableHook(pSetGammaRampTarget) != MH_OK)
				{
					LOG_WARN(kTag, "Couldn't hook SetGammaRamp");
					return;
				}

				uintptr_t applyRamp = Utils::FindPattern(GetModuleHandleA(nullptr), "A1 ? ? ? ? 8B 08 8B 51 54 68 ? ? ? ? 6A 00 6A 00 50 FF D2");
				if (!applyRamp)
				{
					LOG_WARN(kTag, "Couldn't find the game's gamma ramp, the saved brightness only gets fixed once the slider moves");
					return;
				}

				IDirect3DDevice9** ppGameDevice = *reinterpret_cast<IDirect3DDevice9***>(applyRamp + 1);
				const D3DGAMMARAMP* pGameRamp = *reinterpret_cast<const D3DGAMMARAMP**>(applyRamp + 11);

				if (IDirect3DDevice9* pGameDevice = *ppGameDevice)
				{
					D3DGAMMARAMP ramp = *pGameRamp;
					LOG_DEBUG(kTag, "Reapplying the game's current gamma ramp through the hook");
					hkSetGammaRamp(pGameDevice, 0, 0, &ramp);
				}
			}
		}
	}
}
