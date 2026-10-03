#pragma once

#include <Windows.h>
#include <d3d9.h>

namespace Features {
	namespace Graphics {
		namespace D3D9 {

			IDirect3DDevice9* CreateDummyDevice();
			void ReleaseDummyDevice();
		}
	}
}
