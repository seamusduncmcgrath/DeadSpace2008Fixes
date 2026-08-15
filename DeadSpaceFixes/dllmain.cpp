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
#include <emmintrin.h>
#include <tmmintrin.h>
#include <intrin.h>

//MinHook declares MH_STATUS as a C-style unscoped enum, which trips the
//C++ Core Guidelines warning C26812 (Enum.3). It's a third-party header we
//can't change, so disable the warning around the include only.
#pragma warning(push)
#pragma warning(disable: 26812)
#include "MinHook\MinHook.h"
#pragma warning(pop)
#include "SDL3/SDL.h"
#include "TypeDefs.h"
#include "WindowHooks.h"
#include "Utils.h"
#include "InputHooks.h"
#include "Config.h"


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
int g_TargetFPS = 60;
SDL_Gamepad* g_CurrentGamepad = nullptr;

//SkipLoadingScreenDelay: the loading screen timeline driver compares the elapsed
//time against SimGroup+0xB0 (pre-first-tip hold, 15.0s) and SimGroup+0xB4
//(per-tip cycle, 1.0s). The comiss instructions are retargeted to these floats
//so the post-load hold is ~3s instead of ~31s while tips still show. DLL data
//so the address is stable and a config reload can rewrite the values.
float g_LoadingTipPreHold = 2.0f;
float g_LoadingTipCycleHold = 1.0f;

LARGE_INTEGER g_TimerFrequency;
LARGE_INTEGER g_LastFrameTime;


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

//AI:
//NG+ intro cutscene skip: NG+ seeds the 16-byte checkpoint record from object
//state at runtime (a normal new game resolves the checkpoint by name instead),
//so a content rewrite alone can't cover it. We hook the record-creating
//function with MinHook and substitute the "kellion" (landed on Ishimura) checkpoint for the
//"initial" before it's committed. The checkpoint hashes are derived from
//the checkpoint names in the game's save data, so these constants are stable
//across builds.
typedef void(__cdecl* CreateCheckpointRecord_t)(void* arg1, void* arg2, uint32_t rec0, uint32_t rec1, uint32_t rec2, uint32_t rec3);
CreateCheckpointRecord_t oCreateCheckpointRecord = nullptr;

void __cdecl hkCreateCheckpointRecord(void* arg1, void* arg2, uint32_t rec0, uint32_t rec1, uint32_t rec2, uint32_t rec3)
{
	static const uint32_t kCheckpointIntro[4] = { 0x4BC8A99C, 0xD622DBBB, 0x544E4543, 0x53574F4B };
	static const uint32_t kCheckpointIshimura[4] = { 0x4BC78C36, 0x9F71988A, 0x544E4543, 0x53574F4B };

    //r3: human version used memcmp/memcpy for code simplicity
	if (rec0 == kCheckpointIntro[0] && rec1 == kCheckpointIntro[1] &&
		rec2 == kCheckpointIntro[2] && rec3 == kCheckpointIntro[3])
	{
		rec0 = kCheckpointIshimura[0];
		rec1 = kCheckpointIshimura[1];
		rec2 = kCheckpointIshimura[2];
		rec3 = kCheckpointIshimura[3];
	}

	oCreateCheckpointRecord(arg1, arg2, rec0, rec1, rec2, rec3);
}


void InitialiseNetworkHooks()
{
    HMODULE hWinSock = GetModuleHandleA("ws2_32.dll");
    if (hWinSock != nullptr) //GetModuleHandleA can fail, guard against the NULL deref before passing to GetProcAddress
    {
        FARPROC pWSAStartup = GetProcAddress(hWinSock, "WSAStartup");
        if (pWSAStartup)
        {
            MH_CreateHook(pWSAStartup, &hkWSAStartup, reinterpret_cast<LPVOID*>(&OriginalWSAStartup));
            MH_EnableHook(pWSAStartup);
            DEBUG_LOG("WSAStartup hooked, all network/telemetry blocked");
        }
    }
    HMODULE hNetApi = LoadLibraryA("netapi32.dll"); //some games don't properly load this so we LoadLibrary it
    if (hNetApi != nullptr) //LoadLibraryA can also fail, same guard
    {
        FARPROC pNetbios = GetProcAddress(hNetApi, "Netbios");
        if (pNetbios)
        {
            MH_CreateHook(pNetbios, &hkNetbios, reinterpret_cast<LPVOID*>(&OriginalNetbios));
            MH_EnableHook(pNetbios);
            DEBUG_LOG("Netbios also hooked, weird NAT harvester is now blocked");
        }
    }
}

HRESULT APIENTRY hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
    if (g_TargetFPS > 0)
    {
        LARGE_INTEGER currentTime;
        LONGLONG targetTicksPerFrame = g_TimerFrequency.QuadPart / g_TargetFPS;

        while (true)
        {
            QueryPerformanceCounter(&currentTime);

            LONGLONG elapsed = currentTime.QuadPart - g_LastFrameTime.QuadPart;
            if (elapsed >= targetTicksPerFrame)
                break;

            //if were still far away sleep a bit
            if (targetTicksPerFrame - elapsed > g_TimerFrequency.QuadPart / 1000) //>1ms
            {
                Sleep(1);
            }

            else
            {
                YieldProcessor(); //proper fine grain wait, never knew about this till today lol
            }
        }

        g_LastFrameTime.QuadPart += targetTicksPerFrame;

        if (currentTime.QuadPart - g_LastFrameTime.QuadPart > targetTicksPerFrame)
        {
            g_LastFrameTime = currentTime;
        }
    }
    return oEndScene(pDevice);
}


DWORD WINAPI SDLDeviceThread(LPVOID lpParam)
{
    if (!SDL_Init(SDL_INIT_GAMEPAD))
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
                    const char* name = SDL_GetGamepadName(g_CurrentGamepad);
                    DEBUG_LOG("Controller connected: %s", name ? name : "Unknown");
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
    Utils::InitialiseConsole();
#endif

    if (Config::PatchOutDInput8) {
        Input::InitialiseInputHooks();
    }

    if (Config::BorderlessWindowed) {
        Window::InitialiseWindowHooks();
    }

    if (Config::RemoveTelemetry) {
        InitialiseNetworkHooks();
    }

    HMODULE hExe = GetModuleHandleA(nullptr);

    const char* timerSignature = "80 3D ? ? ? ? 00 74 15 8D 54 24 0C 52";
    uintptr_t patternAddress = Utils::FindPattern(hExe, timerSignature);

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

    if (Config::FixVSync) {
        const char* vsyncMenuSig = "BA 02 00 00 00 EB ? 33 D2 89 15";
        uintptr_t vsyncmenuAddress = Utils::FindPattern(hExe, vsyncMenuSig);

        if (vsyncmenuAddress != 0)
        {
            void* patchAddress = reinterpret_cast<void*>(vsyncmenuAddress + 1);
            DEBUG_LOG("Found vsync menu address at 0x%p", patchAddress);

            DWORD oldProtect;
            if (VirtualProtect(patchAddress, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
            {
                *static_cast<BYTE*>(patchAddress) = 0x00;
                VirtualProtect(patchAddress, 1, oldProtect, &oldProtect);
                DEBUG_LOG("Patched menu vsync");
            }
        }
    }

    if (Config::FixSubtitleScale)
    {
        const char* subtitleSignature = "83 7E 1C 00 F3 0F 10 46 44 F3 0F 59 45 18 53 8B 5D 1C";
        uintptr_t subtitleAddress = Utils::FindPattern(hExe, subtitleSignature);

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
    }

    //I figured out I can set my own custom version string on the main menu so why not lol
    const char* versionSignature = "68 ? ? ? ? 6A 64 68 ? ? ? ? E8 ? ? ? ? 83 C4 1C";
    uintptr_t versionAddress = Utils::FindPattern(hExe, versionSignature);
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

    if (Config::SkipLandingCutscene) {
        //AI: A normal new game resolves the checkpoint by name. The "new game"
        //checkpoint name is game data (identical in every build), so locate it
        //by content and rewrite it to the "kellion" (landed on Ishimura) checkpoint name. Both
        //names are the same length, so the null terminator stays put.
        //r3: previous (human) version used code hook and pointer to find that string location similar to version string swap. AI implemented the direct string swap instead 
        uintptr_t checkpointNameAddress = Utils::FindString(hExe, "XCENTKOWSK_C8A99CD_622DBBB_v3");
        if (checkpointNameAddress != 0)
        {
            const char* landingSeenName = "XCENTKOWSK_C78C369_F71988A_v3";
            if (Utils::WriteBytes(checkpointNameAddress, landingSeenName, 29))
                DEBUG_LOG("Skipped intro!");
            else
                DEBUG_LOG("Failed to skip intro!");
        }

        //NG+ variant: NG+ seeds the checkpoint record from object state instead
        //of resolving it by name, so the name rewrite above never runs. Hook the
        //record-creating function and rewrite the New Game checkpoint for the
        //kellion (landed on Ishimura) before it's committed.
        //r3: previous (human) version used to hook the tail of the function, 
        // instead of prologue, finding CP address and writing to it. 
        // in this version old CP is swapped on entry, not going further. 
        const char* ngPlusSig = "83 EC 0C 56 57 E8 ?? ?? ?? ?? 64 A1 2C 00 00 00 8B 30";
        uintptr_t ngPlusAddress = Utils::FindPattern(hExe, ngPlusSig);
        if (ngPlusAddress != 0)
        {
            if (MH_CreateHook(reinterpret_cast<void*>(ngPlusAddress), &hkCreateCheckpointRecord,
                reinterpret_cast<LPVOID*>(&oCreateCheckpointRecord)) == MH_OK &&
                MH_EnableHook(reinterpret_cast<void*>(ngPlusAddress)) == MH_OK)
            {
                DEBUG_LOG("NG+ landing cutscene skip hooked at 0x%X", ngPlusAddress);
            }
            else
            {
                DEBUG_LOG("Failed to hook NG+ landing cutscene skip");
            }
        }
    }

    if (Config::SkipIntroToMainMenu) {
        //SkipIntroToMainMenu: skip the boot intro and jump straight to the main menu.
        //Reverse engineered and implemented by AI.

        //Boot state writes: state 2 and state 3 both become state 8 (attract).
        const char* bootState2Sig = "C7 05 ?? ?? ?? ?? 02 00 00 00 5F 5E 5B 8B E5 5D C2 04 00";
        uintptr_t bootState2 = Utils::FindPattern(hExe, bootState2Sig);
        if (bootState2 != 0) {
            DEBUG_LOG("Found frontend boot state 2 write at 0x%X", bootState2);
            const BYTE attract = 0x08;
            if (Utils::WriteBytes(bootState2 + 6, &attract, sizeof(attract)))
                DEBUG_LOG("Patched boot state 2 to attract state");
        }

        const char* bootState3Sig = "C7 05 ?? ?? ?? ?? 03 00 00 00 5F 5E 5B 8B E5 5D C2 04 00";
        uintptr_t bootState3 = Utils::FindPattern(hExe, bootState3Sig);
        if (bootState3 != 0) {
            DEBUG_LOG("Found frontend boot state 3 write at 0x%X", bootState3);
            const BYTE attract = 0x08;
            if (Utils::WriteBytes(bootState3 + 6, &attract, sizeof(attract)))
                DEBUG_LOG("Patched boot state 3 to attract state");
        }

        //Attract state (case 9): jump from its entry straight to its exit sequence
        const char* attractSig = "8B ? 50 F3 0F 10 ? 54 0F 2F ? 84 02 00 00";
        uintptr_t attractEntry = Utils::FindPattern(hExe, attractSig);
        if (attractEntry != 0) {
            const char* attractExitSig = "A1 ? ? ? ? E8 ? ? ? ? 8B ? 50 0F 57 C0 83 C0 5C F3 0F 11 ? 54";
            uintptr_t attractExit = Utils::FindPattern(hExe, attractExitSig, attractEntry, attractEntry + 0x400);
            if (attractExit != 0) {
                DEBUG_LOG("Found attract state at 0x%X with exit at 0x%X", attractEntry, attractExit);

                BYTE patch[8];
                patch[0] = 0xE9; //JMP
                *reinterpret_cast<uintptr_t*>(patch + 1) = attractExit - attractEntry - 5; //relative offset
                patch[5] = 0x90; //NOP the rest of the overwritten instructions
                patch[6] = 0x90;
                patch[7] = 0x90;
                if (Utils::WriteBytes(attractEntry, patch, sizeof(patch)))
                    DEBUG_LOG("Patched attract state to exit immediately");

            }
        }

        const char* saveStringSignature = "8B 44 24 08 85 C0 74 14 50 8B 44 24 08 68 80 00"; //credit to marker patch for this
        uintptr_t  saveStringAddress = Utils::FindPattern(hExe, saveStringSignature);
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

        if (Config::PatchOutDInput8) {
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

        if (Config::SkipLoadingScreenDelay) {
            //SkipLoadingScreenDelay: shorten the artificial post-load hold on the
            //loading screen instead of removing the tip text. After the level
            //finishes loading, the timeline driver (FUN_00617620) waits
            //SimGroup+0xB0 (15.0s) before showing the first tip, then cycles one
            //tip every SimGroup+0xB4 (1.0s) before hiding the screen. Retarget
            //both comiss hold comparisons (0x00617694 / 0x006176BB) to our own
            //floats so tips appear quickly and the screen cuts ~3s after
            //load-done. Both sites are 7-byte comiss xmm0,[m32] instructions, so
            //the replacement (0F 2F 05 <disp32>) is the same length.
            //Reverse engineered and verified by AI.
            const char* loadingPreHoldSig = "F3 0F 10 46 14 0F 2F 80 B0 00 00 00 76 41 0F 57 C0";
            uintptr_t loadingPreHold = Utils::FindPattern(hExe, loadingPreHoldSig);
            if (loadingPreHold != 0) {
                DEBUG_LOG("Found loading screen pre-tip hold at 0x%X", loadingPreHold + 5);
                BYTE patch[7] = { 0x0F, 0x2F, 0x05 };
                *reinterpret_cast<uint32_t*>(patch + 3) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_LoadingTipPreHold));
                if (Utils::WriteBytes(loadingPreHold + 5, patch, sizeof(patch)))
                    DEBUG_LOG("Patched loading screen pre-tip hold to %.1fs", g_LoadingTipPreHold);
            }

            //SkipLoadingScreenDelay (tip cycle): the per-tip hold. Retarget to our
            //own float the same way so every tip after the first also cuts quickly.
            const char* loadingCycleSig = "8B 56 28 0F 2F 82 B4 00 00 00 76 1A C7 46 24 04";
            uintptr_t loadingCycle = Utils::FindPattern(hExe, loadingCycleSig);
            if (loadingCycle != 0) {
                DEBUG_LOG("Found loading screen tip cycle hold at 0x%X", loadingCycle + 3);
                BYTE patch[7] = { 0x0F, 0x2F, 0x05 };
                *reinterpret_cast<uint32_t*>(patch + 3) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&g_LoadingTipCycleHold));
                if (Utils::WriteBytes(loadingCycle + 3, patch, sizeof(patch)))
                    DEBUG_LOG("Patched loading screen tip cycle hold to %.1fs", g_LoadingTipCycleHold);
            }
        }

        QueryPerformanceFrequency(&g_TimerFrequency);
        QueryPerformanceCounter(&g_LastFrameTime);

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

            if (Config::SafeFPSCap)
            {
                void* pEndSceneTarget = pVTable[42];
                MH_CreateHook(pEndSceneTarget, &hkEndScene, reinterpret_cast<LPVOID*>(&oEndScene));
                MH_EnableHook(pEndSceneTarget);
            }

            //cleanup
            pDummyDevice->Release();
        }

        pD3D->Release();

        return 0;

    }
    return 0;
}

void ApplyStartupVSyncPatch() //this has to run really early or it's broken
{
    HMODULE hExe = GetModuleHandleA(nullptr);
    if (!hExe) return;

    // The signature for MOV EBP, 2
    const char* vsyncStartupSig = "64 A1 ? ? ? ? 8B 08 8B 49 0C BD 02 00 00 00 33 FF";
    uintptr_t vsyncStartupAddress = Utils::FindPattern(hExe, vsyncStartupSig);

    if (vsyncStartupAddress != 0)
    {
        // +12 bytes to hit the '02'
        void* patchAddress = reinterpret_cast<void*>(vsyncStartupAddress + 12);

        DWORD oldProtect;
        if (VirtualProtect(patchAddress, 1, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            *static_cast<BYTE*>(patchAddress) = 0x00; //0x00 uncaps it, 0x01 sets it to 60, 0x02 sets it to 30 (which is what the game normally uses
            VirtualProtect(patchAddress, 1, oldProtect, &oldProtect);
        }
    }
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
        Config::Load(); //really dislike doing it like this, could definitely cause issues.
        if (Config::FixVSync)
        {
            ApplyStartupVSyncPatch();
        }

        CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
    }
    return TRUE;
}