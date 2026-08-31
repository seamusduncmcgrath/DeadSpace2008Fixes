#include "common.h"
#include "Utils.h"

namespace Features {

	namespace Graphics {

		namespace AnisotropicFiltering {

			//Forces 16x anisotropic filtering on all surfaces for crisper textures at
			//oblique angles, effectively free on modern hardware.

			SetSamplerState_t oSetSamplerState = nullptr;

			HRESULT __stdcall hkSetSamplerState(IDirect3DDevice9* pDevice, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
			{
				//checks if the game is trying to set a sampler state
				if (Type == D3DSAMP_MAGFILTER || Type == D3DSAMP_MINFILTER || Type == D3DSAMP_MIPFILTER) //suprisingly clean
				{
					Value = D3DTEXF_ANISOTROPIC;
					oSetSamplerState(pDevice, Sampler, D3DSAMP_MAXANISOTROPY, 16);
				}

				return oSetSamplerState(pDevice, Sampler, Type, Value);
			}

			void Apply(IDirect3DDevice9* pDummyDevice)
			{
				void** pVTable = *reinterpret_cast<void***>(pDummyDevice);
				void* pSetSamplerStateTarget = pVTable[69];

				MH_CreateHook(pSetSamplerStateTarget, &hkSetSamplerState, reinterpret_cast<LPVOID*>(&oSetSamplerState));
				MH_EnableHook(pSetSamplerStateTarget);
			}
		}
	}
}
