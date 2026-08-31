#include "common.h"
#include "D3D9Device.h"
#include "Config.h"
#include "Features/Features.h"

namespace Features {

	namespace Graphics {

		namespace D3D9 {

			IDirect3DDevice9* g_DummyDevice = nullptr;

			IDirect3DDevice9* CreateDummyDevice()
			{
				if (g_DummyDevice)
					return g_DummyDevice;

				//wait for the game window to exist before creating the device
				HWND hwnd = nullptr;
				while (!hwnd) // todo: use window creation hook instead so this function could be used anywhere in order.
				{
					hwnd = FindWindowA("DeadSpaceWndClass", nullptr);
					Sleep(100);
				}

				IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
				if (!pD3D) return nullptr;

				D3DPRESENT_PARAMETERS d3dpp = {};
				d3dpp.Windowed = TRUE;
				d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
				d3dpp.hDeviceWindow = hwnd;

				HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &g_DummyDevice);

				pD3D->Release();

				return g_DummyDevice;
			}

			void ReleaseDummyDevice()
			{
				if (g_DummyDevice)
				{
					g_DummyDevice->Release();
					g_DummyDevice = nullptr;
				}
			}

			//Installs every D3D9 device-hook feature. Waits for the game window, steals
			//the device vtable and attaches the hooks. Make sure to run it last, as it
			//blocks until the window is created.
			void InstallDeviceHooks()
			{
				IDirect3DDevice9* dummy = CreateDummyDevice();
				if (!dummy) return;

				AnisotropicFiltering::Apply(dummy);

				//todo: consider making the fps cap a configurable value
				if (Config::Features::FrameRateCap)
					FrameRateCap::Apply(dummy);

				ReleaseDummyDevice();
			}
		}
	}
}
