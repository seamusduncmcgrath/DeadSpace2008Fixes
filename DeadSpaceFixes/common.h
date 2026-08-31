#pragma once

// Common definitions shared by every source file in this project.
// Include this instead of assembling Windows/SDL/MinHook includes by hand.
// Project root is on the include path, so all files can just `#include "common.h"`.


// ---- Common preprocessor defines ----
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

// ---- Windows / DirectX includes ----
#include <Windows.h>
#include <winsock2.h>
#include <nb30.h>
#include <d3d9.h>
#include <dinput.h>

#include <cstdint>
#include <cstring>

// ---- Third party: must come after system headers ----
#pragma warning(push)
#pragma warning(disable: 26812)
#include "MinHook\MinHook.h"
#pragma warning(pop)
#include "SDL3\SDL.h"


// ---- XINPUT definitions ----
// The game loads the real xinput1_3.dll, and this DLL proxies it, so we can't
// link against the SDK's xinput.h. These are the type/constant definitions we
// need, kept self-contained so nothing else has to provide them.

#define XINPUT_DEVTYPE_GAMEPAD          0x01
#define XINPUT_DEVSUBTYPE_GAMEPAD       0x01

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y                0x8000

typedef struct _XINPUT_GAMEPAD {
    WORD  wButtons;
    BYTE  bLeftTrigger;
    BYTE  bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
} XINPUT_GAMEPAD, * PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE {
    DWORD          dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
} XINPUT_STATE, * PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION {
    WORD wLeftMotorSpeed;
    WORD wRightMotorSpeed;
} XINPUT_VIBRATION, * PXINPUT_VIBRATION;

typedef struct _XINPUT_CAPABILITIES {
    BYTE             Type;
    BYTE             SubType;
    WORD             Flags;
    XINPUT_GAMEPAD   Gamepad;
    XINPUT_VIBRATION Vibration;
} XINPUT_CAPABILITIES, * PXINPUT_CAPABILITIES;


// ---- Function pointer typedefs for hooked engine / system APIs ----
typedef HRESULT(__stdcall* SetSamplerState_t)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);

typedef HWND(WINAPI* CreateWindowExA_t)(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

typedef BOOL(WINAPI* AdjustWindowRect_t)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);

typedef BOOL(WINAPI* AdjustWindowRectEx_t)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);

typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

typedef HRESULT(__stdcall* EnumDevices_t)(IDirectInput8*, DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD);

typedef DWORD_PTR(WINAPI* SetThreadAffinityMask_t)(HANDLE hThread, DWORD_PTR dwThreadAffinityMask);

typedef DWORD(WINAPI* SetThreadIdealProcessor_t)(HANDLE hThread, DWORD dwIdealProcessor);

typedef int(WINAPI* WSAStartup_t)(WORD wVersionRequested, LPWSADATA lpWSAData);

typedef UCHAR(WINAPI* Netbios_t)(PNCB pncb);

typedef HRESULT(APIENTRY* EndScene_t)(IDirect3DDevice9*);

typedef errno_t(*SaveStringCopy_t)(wchar_t* dest, wchar_t* src);

typedef bool(*ShouldUseDirectInput_t)();

typedef DWORD(WINAPI* XInputGetState_t)(DWORD dwUserIndex, XINPUT_STATE* pState);

typedef DWORD(WINAPI* XInputSetState_t)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

typedef DWORD(WINAPI* XInputGetCapabilities_t)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);

typedef HWND(WINAPI* GetForegroundWindow_t)();


// ---- Exported proxy functions (from the .def file) ----
// Declared here so SdlGamepad.cpp can define them and nothing else redefines them.
extern "C" DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState);
extern "C" DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
extern "C" DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
