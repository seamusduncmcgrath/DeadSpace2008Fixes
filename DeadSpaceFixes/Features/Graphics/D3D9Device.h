#pragma once

// Internal helper shared by the D3D9 device-hook features (anisotropic filtering
// and frame-rate cap). Both hook methods on the D3D9 device vtable, so they share
// the "wait for the game window, create a dummy device, steal its vtable" step.

#include <Windows.h>
#include <d3d9.h>

namespace Features {
	namespace Graphics {
		namespace D3D9 {
			// Waits for the game window to exist and creates a dummy D3D9 device
			// whose vtable the hook features can attach to. Caches the dummy device
			// so it is only created once. Returns nullptr on failure.
			IDirect3DDevice9* CreateDummyDevice();
			// Releases the cached dummy device. Call after installing hooks.
			void ReleaseDummyDevice();
		}
	}
}
