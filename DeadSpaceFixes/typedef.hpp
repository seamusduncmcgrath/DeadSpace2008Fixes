#pragma once
#include <windows.h>
#include <d3d9.h>

typedef HRESULT(__stdcall* SetSamplerState_t)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);
SetSamplerState_t oSetSamplerState = nullptr;

typedef HWND(WINAPI* CreateWindowExA_t)(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);
CreateWindowExA_t oCreateWindowExA = nullptr;