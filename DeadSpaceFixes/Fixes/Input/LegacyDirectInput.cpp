#include "common.h"
#include "Utils.h"

namespace Fixes {

	namespace Input {

		namespace LegacyDirectInput {

			//Removes the game's legacy DirectInput8 controller handling to cut startup
			//times. Two mechanisms:
			//  1. Hook DirectInput8Create and only enumerate mouse + keyboard, skipping
			//     the very slow scan of every connected HID/legacy joystick.
			//  2. Lobotomize the engine's ShouldUseDirectInput() decision so it stops
			//     running its expensive XInput-vs-DInput compatibility check.

			DirectInput8Create_t oDirectInput8Create = nullptr;
			EnumDevices_t oEnumDevices = nullptr;
			ShouldUseDirectInput_t oShouldUseDirectInput = nullptr;

			HRESULT __stdcall hkEnumDevices(IDirectInput8* pDI, DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags)
			{
				//We only scan for mouse and keyboard, should reduce startup times by like 5 seconds.
				//Controllers will be fine since they use XInput.
				oEnumDevices(pDI, DI8DEVCLASS_POINTER, lpCallback, pvRef, dwFlags);
				oEnumDevices(pDI, DI8DEVCLASS_KEYBOARD, lpCallback, pvRef, dwFlags);
				return DI_OK;
			}

			HRESULT WINAPI hkDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter)
			{
				HRESULT hr = oDirectInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);

				if (SUCCEEDED(hr) && ppvOut && *ppvOut)
				{
					//steal the vtable
					void** pVtable = *reinterpret_cast<void***>(*ppvOut);
					void* pEnumDevicesTarget = pVtable[4];

					//we only want to create this hook once
					if (oEnumDevices == nullptr)
					{
						MH_CreateHook(pEnumDevicesTarget, &hkEnumDevices, reinterpret_cast<LPVOID*>(&oEnumDevices));
						MH_EnableHook(pEnumDevicesTarget);
					}
				}
				//hand it back to the engine
				return hr;
			}

			void HookDirectInput8Create()
			{
				HMODULE hDinput8 = LoadLibraryA("dinput8.dll");
				if (hDinput8)
				{
					LPVOID pDirectInput8Create = GetProcAddress(hDinput8, "DirectInput8Create");
					MH_CreateHook(pDirectInput8Create, &hkDirectInput8Create, reinterpret_cast<LPVOID*>(&oDirectInput8Create));
					MH_EnableHook(pDirectInput8Create);
				}
			}

			//This function is some straight ass visceral cooked up, it checks if stuff
			//is DInput or XInput, but since DInput is dead due to prev hooks, we can just
			//lobotomise it.
			bool hkShouldUseDirectInput()
			{
				return false; //wtf? shouldn't this be true. weird but now this slow ass function is dead
			}

			void LobotomiseShouldUseDirectInput(HMODULE hExe)
			{
				const char* useDirectInputSignature = "81 EC 84 00 00 00 53 56 57 33 DB 6A 4C 8D 44 24 48 53 50 89 5C 24 20 89 5C 24 1C 89 5C 24 4C E8 ? ? ? ?";
				uintptr_t useDirectInputAddress = Utils::FindPattern(hExe, useDirectInputSignature);

				if (useDirectInputAddress != 0)
				{
					void* pUseDirectInputTarget = reinterpret_cast<void*>(useDirectInputAddress);

					if (MH_CreateHook(pUseDirectInputTarget, &hkShouldUseDirectInput, reinterpret_cast<LPVOID*>(&oShouldUseDirectInput)) == MH_OK)
					{
						MH_EnableHook(pUseDirectInputTarget);
						DEBUG_LOG("Removed terrible controller API checker");
					}
				}
			}

			void Apply(HMODULE hExe)
			{
				HookDirectInput8Create();
				LobotomiseShouldUseDirectInput(hExe);
			}
		}
	}
}
