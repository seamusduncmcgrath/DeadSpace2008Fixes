#pragma once


#include <Windows.h>
#include <d3d9.h>

namespace Features {

	namespace Input {
		namespace SdlGamepad {
			void StartThread();
			void Shutdown();
		}
	}

	namespace Graphics {
		namespace AnisotropicFiltering {
			void Apply(IDirect3DDevice9* pDummyDevice);
		}
		namespace FrameRateCap {
			void Apply(IDirect3DDevice9* pDummyDevice);
		}
		namespace D3D9 {
			void InstallDeviceHooks();
		}
	}

}
