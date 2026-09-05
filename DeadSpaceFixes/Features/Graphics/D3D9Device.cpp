#include "common.h"
#include "D3D9Device.h"
#include "Config.h"
#include "Features/Features.h"

namespace Features {

	namespace Graphics {

		namespace D3D9 {

			IDirect3DDevice9* g_DummyDevice = nullptr;

			namespace {

				const wchar_t* const kHelperClass = L"DeadSpaceFixesHelperWnd";
				HWND g_HelperWindow = nullptr;

				// A private, hidden top-level window used only as the focus/device
				// window when creating the dummy IDirect3DDevice9. Its sole purpose is
				// to hand us the D3D9 device vtable, and any valid HWND works — no need
				// to wait for (or depend on) the game's own window. Registered once,
				// created lazily.
				HWND GetHelperWindow()
				{
					if (g_HelperWindow)
						return g_HelperWindow;

					HINSTANCE instance = GetModuleHandleA(nullptr);
					WNDCLASSW wc = {};
					wc.lpfnWndProc = DefWindowProcW;
					wc.hInstance = instance;
					wc.lpszClassName = kHelperClass;
					RegisterClassW(&wc);

					g_HelperWindow = CreateWindowExW(0, kHelperClass, L"DeadSpaceFixes helper window",
						WS_POPUP, 0, 0, 8, 8, nullptr, nullptr, instance, nullptr);
					return g_HelperWindow;
				}
			}

			IDirect3DDevice9* CreateDummyDevice()
			{
				if (g_DummyDevice)
					return g_DummyDevice;

				HWND hwnd = GetHelperWindow();

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

			//Installs every D3D9 device-hook feature. Creates a transient dummy device on
			//a private hidden helper window to steal the device vtable and attach the
			//hooks; because all IDirect3DDevice9 instances share that vtable, the game's
			//own device gets the hooks regardless of when this runs, so it can be applied
			//anywhere and never blocks.
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
