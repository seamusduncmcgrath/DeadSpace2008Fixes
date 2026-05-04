#include <windows.h>
#include <d3d9.h>
#include <dinput.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstdint>
#include "MinHook.h"
#include "TypeDefs.h"
#include "WindowHooks.h"
#include "Utils.h"
#include "InputHooks.h"

//Globals
SetSamplerState_t oSetSamplerState = nullptr;

uintptr_t subtitleHookReturn = 0;
float subtitleScale = 1.0f;

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

    
__declspec(naked) void hkSubtitleScale() //note to self comment well
{
    __asm //I fucking hate assembly
    {
        pushfd

        cmp edi, 0 //is it the inventory or other menus?
        je original_code //if yes skip to original code

        //if it's a subtitle replace scale param
        push eax
        mov eax, dword ptr [subtitleScale]
        mov dword ptr [ebp+0x18], eax
        pop eax

    original_code:
        popfd //restore cpu flags

        mulss xmm0, dword ptr [ebp+0x18] //original instruction

        jmp [subtitleHookReturn] //jump back to engine
    }
}

DWORD WINAPI MainThread(LPVOID)
{
    #ifdef _DEBUG
    InitialiseConsole();
    #endif

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
        DEBUG_LOG("Signature for high precision timer found at 0x%X", patternAddress);
        void* patchAddress = reinterpret_cast<void*>(patternAddress + 7);

        DWORD oldProtect;
        if (VirtualProtect(patchAddress, 2, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            BYTE* pByte = static_cast<BYTE*>(patchAddress);
            pByte[0] = 0x90;
            pByte[1] = 0x90;

            VirtualProtect(patchAddress, 2, oldProtect, &oldProtect);
            DEBUG_LOG("Timer patched");
        }
    }

    const char* subtitleSignature = "83 7E 1C 00 F3 0F 10 46 44 F3 0F 59 45 18 53 8B 5D 1C";
    uintptr_t subtitleAddress = FindPattern(hExe, subtitleSignature);

    if (subtitleAddress != 0)
    {
        void* patchAddress = reinterpret_cast<void*>(subtitleAddress + 9);
        DEBUG_LOG("Found subtitle scale hook at 0x%p", patchAddress);

        //calculate the correct scale
        //DS1 was only designed for up to 720p, so subtitles don't scale above it, so we just (try to) correctly scale it here
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        subtitleScale = screenHeight / 1080.0f; //720 would be better but I'm to lazy to figure it out

        DEBUG_LOG("Subtitle scale is now %f", subtitleScale);

        subtitleHookReturn = reinterpret_cast<uintptr_t>(patchAddress) + 5;

        DWORD oldProtect;
        if (VirtualProtect(patchAddress, 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            BYTE* pByte = reinterpret_cast<BYTE*>(patchAddress);

            //write the E9 JMP
            pByte[0] = 0xE9;

            *reinterpret_cast<uintptr_t*>(pByte + 1) = reinterpret_cast<uintptr_t>(&hkSubtitleScale) - reinterpret_cast<uintptr_t>(patchAddress) - 5;

            VirtualProtect(patchAddress, 5, oldProtect, &oldProtect);
            DEBUG_LOG("Subtitles scaled");
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
        DEBUG_LOG("Core/Thread count is %d", processorCount);
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