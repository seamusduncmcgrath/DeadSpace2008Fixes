#pragma once
#include <winsock2.h>
#include <windows.h>
#include <nb30.h>
#include <d3d9.h>
#include <dinput.h>

typedef HRESULT(__stdcall* SetSamplerState_t)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);

typedef HWND(WINAPI* CreateWindowExA_t)(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

typedef BOOL(WINAPI* AdjustWindowRect_t)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);

typedef BOOL(WINAPI* AdjustWindowRectEx_t)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);

typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

typedef HRESULT(__stdcall* EnumDevices_t)(IDirectInput8*, DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD);

typedef DWORD_PTR(WINAPI* SetThreadAffinityMask_t)(HANDLE hThread, DWORD_PTR dwThreadAffinityMask);

typedef DWORD(WINAPI* SetThreadIdealProcessor_t)(HANDLE hThread, DWORD dwIdealProcessor);

typedef int (WINAPI* WSAStartup_t)(WORD wVersionRequested, LPWSADATA lpWSAData);

typedef UCHAR(WINAPI* Netbios_t)(PNCB pncb);

typedef HRESULT(APIENTRY* EndScene_t)(IDirect3DDevice9*);

typedef errno_t(*SaveStringCopy_t)(wchar_t* dest, wchar_t* src); //FUN_008ea570