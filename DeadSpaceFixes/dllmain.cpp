#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <nb30.h>
#include <d3d9.h>
#include <dinput.h>
#include <string>
#include <iostream>
#include <vector>
#include <cstdint>

#include "include\MinHook\MinHook.h"
#include "SDL3/SDL.h"
#include "TypeDefs.h"
#include "WindowHooks.h"
#include "Utils.h"
#include "InputHooks.h"


//Globals
SetSamplerState_t oSetSamplerState = nullptr;
SetThreadAffinityMask_t oSetThreadAffinityMask = nullptr;
SetThreadIdealProcessor_t oSetThreadIdealProcessor = nullptr;
WSAStartup_t OriginalWSAStartup = nullptr;
Netbios_t OriginalNetbios = nullptr;
EndScene_t oEndScene = nullptr;
SaveStringCopy_t oSaveStringCopy = nullptr;
uintptr_t subtitleHookReturn = 0;
XInputGetState_t oXInputGetState = nullptr;
XInputSetState_t oXInputSetState = nullptr;
XInputGetCapabilities_t oXInputGetCapabilities = nullptr;
ShouldUseDirectInput_t oShouldUseDirectInput = nullptr; //wish these weren't such a pain so this could be in inputhooks.cpp

float subtitleScale = 1.0f; //feel like 0.8 is a better baseline, subtitles clip out less then
SDL_Gamepad* g_CurrentGamepad = nullptr;


//proxy dll
extern "C" DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    if (dwUserIndex == 0 && g_CurrentGamepad != nullptr && SDL_GamepadConnected(g_CurrentGamepad))
    {
        memset(pState, 0, sizeof(XINPUT_STATE)); //clear the games struct so we start fresh

        static DWORD s_PacketNumber = 0;
        pState->dwPacketNumber = s_PacketNumber++;

        //map the buttons
        WORD buttons = 0;
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_SOUTH)) buttons |= XINPUT_GAMEPAD_A; //cross/b
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_EAST))  buttons |= XINPUT_GAMEPAD_B; //circle/a
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_WEST))  buttons |= XINPUT_GAMEPAD_X; //square/y
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_NORTH)) buttons |= XINPUT_GAMEPAD_Y; //triangle/x

        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))    buttons |= XINPUT_GAMEPAD_DPAD_UP;
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;

        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))  buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER; //L1
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER; //R1
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_START)) buttons |= XINPUT_GAMEPAD_START; //options/plus

        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK))  buttons |= XINPUT_GAMEPAD_LEFT_THUMB;  // L3 / left stick click, nearlly forgot about these
        if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB; // R3 / right stick click

        pState->Gamepad.wButtons = buttons;

        //map the triggers
        int16_t leftTriggerSDL = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        int16_t rightTriggerSDL = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

        pState->Gamepad.bLeftTrigger = (BYTE)((leftTriggerSDL / 32767.0f) * 255.0f);
        pState->Gamepad.bRightTrigger = (BYTE)((rightTriggerSDL / 32767.0f) * 255.0f);

        //map the joysticks
        pState->Gamepad.sThumbLX = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFTX);
        pState->Gamepad.sThumbRX = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHTX);

        int16_t sdlLeftY = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFTY); //we need to fetch the raw y values or there cooked
        int16_t sdlRightY = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHTY);

        pState->Gamepad.sThumbLY = (sdlLeftY == -32768) ? 32767 : -sdlLeftY; //then  we safely invert the y values here
        pState->Gamepad.sThumbRY = (sdlRightY == -32768) ? 32767 : -sdlRightY;

        return ERROR_SUCCESS;
    }
    if (oXInputGetState) return oXInputGetState(dwUserIndex, pState);
    return ERROR_DEVICE_NOT_CONNECTED;
}


extern "C" DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
    if (dwUserIndex == 0 && g_CurrentGamepad != nullptr && SDL_GamepadConnected(g_CurrentGamepad))
    {
        Uint16 lowFreq = pVibration->wLeftMotorSpeed; //heavy rumble 
        Uint16 highFreq = pVibration->wRightMotorSpeed; //light rumble

        SDL_RumbleGamepad(g_CurrentGamepad, lowFreq, highFreq, 5000); //send the rumble to the controller for 5 seconds, the game should stop it early

        return ERROR_SUCCESS;
    }
    if (oXInputSetState) return oXInputSetState(dwUserIndex, pVibration);
    return ERROR_DEVICE_NOT_CONNECTED;
}


extern "C" DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities) //maybe rewrite soon? Don't think this is great
{
    if (dwUserIndex == 0 && g_CurrentGamepad != nullptr && SDL_GamepadConnected(g_CurrentGamepad))
    {
        memset(pCapabilities, 0, sizeof(XINPUT_CAPABILITIES));

        pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
        pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
        pCapabilities->Flags = 0;

        pCapabilities->Gamepad.wButtons = 0xFFFF;
        pCapabilities->Gamepad.bLeftTrigger = 255;
        pCapabilities->Gamepad.bRightTrigger = 255;
        pCapabilities->Gamepad.sThumbLX = 32767;
        pCapabilities->Gamepad.sThumbLY = 32767;
        pCapabilities->Gamepad.sThumbRX = 32767;
        pCapabilities->Gamepad.sThumbRY = 32767;

        pCapabilities->Vibration.wLeftMotorSpeed = 65535;
        pCapabilities->Vibration.wRightMotorSpeed = 65535;

        return ERROR_SUCCESS;
    }
    if (oXInputGetCapabilities) return oXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);
    return ERROR_DEVICE_NOT_CONNECTED;
}


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


//these 2 hooks kill all network/telemetry stuff, game should start a bit faster and be fully offline now
int WINAPI hkWSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData)
{
    //we return WSASYSNOTREADY (10091) here so the game thinks the network is unavailable
    return 10091;
}


UCHAR WINAPI hkNetbios(PNCB pncb)
{
    //we return NRC_SYSTEM (0x40) here so it can't scan network devices
    return 0x40;
}


errno_t hkSaveStringCopy(wchar_t* dest, wchar_t* src) //just making the save file string handling a bit better
{
    if (src != nullptr)
    {
        //instead of _wcscpy_s which aborts the game on overflow we use wcsncpy to truncate the path to 127 chars
        //doesn't fix really anything but is safer
        wcsncpy(dest, src, 127);

        dest[127] = L'\0'; //ensure string is null terminated
        return 0;
    }
    //theres a bug with the game where it clears 128 bytes rather than 128 wide characters (256 bytes), this should fix it
    memset(dest, 0, 128 * sizeof(wchar_t));
    return -1;
}


//this function is some straight ass viceral cooked up, it checks if stuff is DInput or XInput,
//but since DInput is dead due to prev hooks, we can just lobotomise it
bool hkShouldUseDirectInput()
{
    return false; //wtf? shouldn't this be true. weird but now this slow ass function is dead
}

    
__declspec(naked) void hkSubtitleScale() //note to self comment well
{
    __asm
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


void InitialiseHooks()
{
    HMODULE hWinSock = GetModuleHandleA("ws2_32.dll");
    FARPROC pWSAStartup = GetProcAddress(hWinSock, "WSAStartup");
    if (pWSAStartup)
    {
        MH_CreateHook(pWSAStartup, &hkWSAStartup, reinterpret_cast<LPVOID*>(&OriginalWSAStartup));
        MH_EnableHook(pWSAStartup);
        DEBUG_LOG("WSAStartup hooked, all network/telemetry blocked");
    }
    HMODULE hNetApi = LoadLibraryA("netapi32.dll"); //some games don't properly load this so we LoadLibrary it
    FARPROC pNetbios = GetProcAddress(hNetApi, "Netbios");
    if (pNetbios)
    {
        MH_CreateHook(pNetbios, &hkNetbios, reinterpret_cast<LPVOID*>(&OriginalNetbios));
        MH_EnableHook(pNetbios);
        DEBUG_LOG("Netbios also hooked, weird NAT harvester is now blocked");
    }
}


DWORD WINAPI SDLDeviceThread(LPVOID lpParam)
{
    if (SDL_Init(SDL_INIT_GAMEPAD) < 0)
    {
        DEBUG_LOG("SDL failed to init!");
        return 1;
    }

    SDL_Event event;

    while (true) //this is ass, it's really messy
    {   
        //SDL_WaitEventTimeout pumps the event queue without burning your CPU, it checks for events every 10 milliseconds
        while (SDL_WaitEventTimeout(&event, 10))
        {
            if (event.type == SDL_EVENT_GAMEPAD_ADDED) //controller plugged in
            {
                if (g_CurrentGamepad == nullptr)
                {
                    g_CurrentGamepad = SDL_OpenGamepad(event.gdevice.which); //we open the controller plugged in here
                    SDL_SetGamepadLED(g_CurrentGamepad, 0, 255, 255); //idk i just like the leds kinda like issacs health bar
                }
            }
            else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) //controller removed
            {
                SDL_Gamepad* disconnectedPad = SDL_GetGamepadFromID(event.gdevice.which);
                if (disconnectedPad == g_CurrentGamepad)
                {
                    //clean up memory
                    SDL_CloseGamepad(g_CurrentGamepad);
                    g_CurrentGamepad = nullptr;
                }
            }
        }
    }
    return 0;
}


DWORD WINAPI MainThread(LPVOID)
{
    //proxy DLL stuff
    char syspath[MAX_PATH];
    GetSystemDirectoryA(syspath, MAX_PATH);
    strcat_s(syspath, "\\xinput1_3.dll");

    HMODULE hRealXInput = LoadLibraryA(syspath);
    if (hRealXInput)
    {
        oXInputGetState = (XInputGetState_t)GetProcAddress(hRealXInput, "XInputGetState");
        oXInputSetState = (XInputSetState_t)GetProcAddress(hRealXInput, "XInputSetState");
        oXInputGetCapabilities = (XInputGetCapabilities_t)GetProcAddress(hRealXInput, "XInputGetCapabilities");
    }

    CreateThread(nullptr, 0, SDLDeviceThread, nullptr, 0, nullptr);

    #ifdef _DEBUG
    InitialiseConsole();
    #endif

    InitialiseInputHooks();
    InitialiseWindowHooks();
    InitialiseHooks();

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
        //subtitleScale = screenHeight / 1080.0f; //720 would be better but I'm to lazy to figure it out

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

    //I figured out I can set my own custom version string on the main menu so why not lol
    const char* versionSignature = "68 ? ? ? ? 6A 64 68 ? ? ? ? E8 ? ? ? ? 83 C4 1C";
    uintptr_t versionAddress = FindPattern(hExe, versionSignature);
    if (versionAddress != 0)
    {
        char* pVersionString = *reinterpret_cast<char**>(versionAddress + 8);
        DEBUG_LOG("Found version number at 0x%p", pVersionString);
        DEBUG_LOG("Game version is %s", pVersionString);
        
        const char* customVersion = "DeadSpaceFixes Installed!";

        DWORD oldProtect;
        if (VirtualProtect(pVersionString, 100, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            strcpy_s(pVersionString, 100, customVersion);
            VirtualProtect(pVersionString, 100, oldProtect, &oldProtect);
            DEBUG_LOG("Set custom game version string to %s", customVersion);
        }
    }

    const char* saveStringSignature = "8B 44 24 08 85 C0 74 14 50 8B 44 24 08 68 80 00"; //credit to marker patch for this
    uintptr_t  saveStringAddress = FindPattern(hExe, saveStringSignature);
    if (saveStringAddress != 0)
    {
        void* pSaveCopyTarget = reinterpret_cast<void*>(saveStringAddress);
        DEBUG_LOG("Found save string handling at 0x%p", pSaveCopyTarget);

        if (MH_CreateHook(pSaveCopyTarget, &hkSaveStringCopy, reinterpret_cast<LPVOID*>(&oSaveStringCopy)) == MH_OK)
        {
            MH_EnableHook(pSaveCopyTarget);
            DEBUG_LOG("Save string handling hooked, should be safer");
        }
    }

    const char* useDirectInputSignature = "81 EC 84 00 00 00 53 56 57 33 DB 6A 4C 8D 44 24 48 53 50 89 5C 24 20 89 5C 24 1C 89 5C 24 4C E8 ? ? ? ?";
    uintptr_t useDirectInputAddress = FindPattern(hExe, useDirectInputSignature);

    if (useDirectInputAddress != 0)
    {
        void* pUseDirectInputTarget = reinterpret_cast<void*>(useDirectInputAddress);

        if (MH_CreateHook(pUseDirectInputTarget, &hkShouldUseDirectInput, reinterpret_cast<LPVOID*>(&oShouldUseDirectInput)) == MH_OK)
        {
            MH_EnableHook(pUseDirectInputTarget);
            DEBUG_LOG("Removed terrible controller API checker");
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

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) 
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
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

        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}