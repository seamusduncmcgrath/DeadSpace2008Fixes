#pragma once
#include <windows.h>

void InitialiseWindowHooks();

HWND WINAPI hkCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

BOOL WINAPI hkAdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);

BOOL WINAPI hkAdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle);