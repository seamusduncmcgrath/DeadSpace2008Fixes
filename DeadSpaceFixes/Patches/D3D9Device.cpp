#include "common.h"
#include "Utils.h"
#include "D3D9Device.h"
#include "Config.h"
#include "Patches/Patches.h"

namespace Features {

	namespace Graphics {

		namespace D3D9 {

			IDirect3DDevice9* g_DummyDevice = nullptr;

			namespace {

				const char* const kTag = "[Features/Graphics/D3D9]";
				const wchar_t* const kHelperClass = L"DeadSpaceFixesHelperWnd";

				IDirect3DDevice9** FindGameDevicePointer()
				{
					uintptr_t match = Utils::FindPattern(GetModuleHandleA(nullptr), "A1 ? ? ? ? 8B 08 8B 51 54 68 ? ? ? ? 6A 00 6A 00 50 FF D2");
					return match ? *reinterpret_cast<IDirect3DDevice9***>(match + 1) : nullptr;
				}

				IDirect3DDevice9* ReadGameDevice(IDirect3DDevice9** ppGameDevice)
				{
					return ppGameDevice ? *reinterpret_cast<IDirect3DDevice9* volatile*>(ppGameDevice) : nullptr;
				}
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
				if (FAILED(hr))
				{
					LOG_DEBUG(kTag, "Couldn't create the dummy device (0x%08X)", static_cast<unsigned int>(hr));
					g_DummyDevice = nullptr;
				}

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

			void InstallDeviceHooks()
			{
				IDirect3DDevice9** ppGameDevice = FindGameDevicePointer();
				IDirect3DDevice9* device = ReadGameDevice(ppGameDevice);
				bool usingDummy = false;

				if (!device)
				{
					device = CreateDummyDevice();
					usingDummy = device != nullptr;
				}

				//dummy failed, so the game is most likely partway through creating its fullscreen device, give it a few seconds
				for (int i = 0; !device && ppGameDevice && i < 500; i++)
				{
					Sleep(10);
					device = ReadGameDevice(ppGameDevice);
				}

				if (!device)
				{
					LOG_WARN(kTag, "Couldn't get a D3D9 device, the D3D9 hooks (brightness, anisotropic filtering, frame rate cap) aren't installed");
					return;
				}

				LOG_DEBUG(kTag, "Installing the D3D9 hooks through the %s device", usingDummy ? "dummy" : "game's");

				Fixes::Graphics::FixFullscreenBrightness::Apply(device);

				AnisotropicFiltering::Apply(device);

				//todo: consider making the fps cap a configurable value
				if (Config::Features::FrameRateCap)
					FrameRateCap::Apply(device);

				//never release the game's device, only our own dummy
				if (usingDummy)
					ReleaseDummyDevice();
			}
		}
	}
}
