#include <windows.h>
#include <d3d9.h>
#include <dinput.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstdint>
#include "minhook.h"
#include "typedefs.h"
#include "windowhooks.h"

//Globals
SetSamplerState_t oSetSamplerState = nullptr;
DirectInput8Create_t oDirectInput8Create = nullptr;
EnumDevices_t oEnumDevices = nullptr;

//this forces anisitropic filtering to 16x on all surfaces
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


uintptr_t FindPattern(HMODULE hModule, const char* signature)
{
    static auto patternToByte = [](const char* pattern) {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?') ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
        };

    auto dosHeader = (PIMAGE_DOS_HEADER)hModule;
    auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)hModule + dosHeader->e_lfanew);

    auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
    auto patternBytes = patternToByte(signature);
    auto scanBytes = reinterpret_cast<std::uint8_t*>(hModule);

    auto s = patternBytes.size();
    auto d = patternBytes.data();

    for (auto i = 0ul; i < sizeOfImage - s; ++i) {
        bool found = true;
        for (auto j = 0ul; j < s; ++j) {
            if (scanBytes[i + j] != d[j] && d[j] != -1) {
                found = false;
                break;
            }
        }
        if (found) return reinterpret_cast<uintptr_t>(&scanBytes[i]);
    }
    return 0;
}
    

DWORD WINAPI MainThread(LPVOID)
{
    InitialiseInputHooks();
    InitialiseWindowHooks();

    HMODULE hExe = GetModuleHandleA(nullptr);

    const char* timerSignature = "80 3D ? ? ? ? 00 74 15 8D 54 24 0C 52";
    uintptr_t patternAddress = FindPattern(hExe, timerSignature);

    //high precision timer fix, can fix some of the issues with high framerates. Should still cap to 120-180 max, as 200-300 the issues come back
    //crazy because the reason why this even works is they had a flag toggled to 0 that makes the game use GetTickCount(), but if you set it to 1 it uses QueryPerformanceCounter()
    //which is much more precise and works much better at high FPS, they probably did this due to a issue where QueryPerformanceCounter() would desync and drift on old AMD Athlon X2 CPU's when the game came out

    if (patternAddress != 0)
    {
        void* patchAddress = reinterpret_cast<void*>(patternAddress + 7);

        DWORD oldProtect;
        if (VirtualProtect(patchAddress, 2, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            BYTE* pByte = static_cast<BYTE*>(patchAddress);
            pByte[0] = 0x90;
            pByte[1] = 0x90;

            VirtualProtect(patchAddress, 2, oldProtect, &oldProtect);
        }
    }

    //wait for the window
    HWND hwnd = nullptr;

    while (!hwnd)
    {
        //could be done better
        hwnd = FindWindowA("DeadSpaceWndClass", nullptr);
        Sleep(100);
    }

    //create a dummy D3D9 device to steal vtable
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return 0;

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hwnd;

    IDirect3DDevice9* pDummyDevice = nullptr;
    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);

    if (SUCCEEDED(hr))
    {
        void** pVTable = *reinterpret_cast<void***>(pDummyDevice);
        
        void* pSetSamplerStateTarget = pVTable[69];

        //create and enable hook
        MH_CreateHook(pSetSamplerStateTarget, &hkSetSamplerState, reinterpret_cast<LPVOID*>(&oSetSamplerState));
        MH_EnableHook(pSetSamplerStateTarget);

        //cleanup
        pDummyDevice->Release();
    }

    pD3D->Release();

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        //CPU affinity fix
        DWORD processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

        //on DS1 if the CPU core/thread count is above 8 it causes crashes, so this caps it to 8
        if (processorCount > 8) //might work at 10? Lots of people say the issue is with 10 or 8
        {
            DWORD_PTR affinityMask = 0xFF;
            SetProcessAffinityMask(GetCurrentProcess(), affinityMask);
        }

        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}