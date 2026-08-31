#pragma once

// New-ability features: add capability the game didn't have before. Each feature
// is a self-contained .cpp file (listed in the project) exposing an `Apply(...)`
// entry point declared here.

#include <Windows.h>
#include <d3d9.h>

namespace Features {

	namespace Input {
		namespace SdlGamepad {
			// Starts the SDL device thread that manages the SDL gamepad.
			void StartThread();
			// Tears down SDL gamepad support (called on DLL detach).
			void Shutdown();
		}
	}

	namespace Graphics {
		namespace AnisotropicFiltering {
			// Hooks the sampler-state calls on the given device to force 16x anisotropy.
			void Apply(IDirect3DDevice9* pDummyDevice);
		}
		namespace FrameRateCap {
			// Hooks EndScene on the given device to cap the framerate.
			void Apply(IDirect3DDevice9* pDummyDevice);
		}
		namespace D3D9 {
			// Installs every D3D9 device-hook feature. Waits for the game window then
			// steals the device vtable and attaches the aniso + frame-rate-cap hooks.
			void InstallDeviceHooks();
		}
	}

}
