#include <Windows.h>
#include "minhook.h"
#include "InputHooks.h"
#include "TypeDefs.h"


DirectInput8Create_t oDirectInput8Create = nullptr;
EnumDevices_t oEnumDevices = nullptr;


HRESULT __stdcall hkEnumDevices(IDirectInput8* pDI, DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags)
{
    //we only scan for mouse and keyboard, should reduce startup times by like 5 seconds. Controllers will be fine since they use XInput
    oEnumDevices(pDI, DI8DEVCLASS_POINTER, lpCallback, pvRef, dwFlags);

    oEnumDevices(pDI, DI8DEVCLASS_KEYBOARD, lpCallback, pvRef, dwFlags);

    return DI_OK;
}

//needed to skip dinput enum devices
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


void InitialiseInputHooks()
{
    if (MH_Initialize() != MH_OK) return;

    //dinput8 hook
    HMODULE hDinput8 = LoadLibraryA("dinput8.dll");
    if (hDinput8)
    {
        LPVOID pDirectInput8Create = GetProcAddress(hDinput8, "DirectInput8Create");
        MH_CreateHook(pDirectInput8Create, &hkDirectInput8Create, reinterpret_cast<LPVOID*>(&oDirectInput8Create));
        MH_EnableHook(pDirectInput8Create);
    }
}