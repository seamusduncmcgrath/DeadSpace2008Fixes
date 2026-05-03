#include <Windows.h>
#include <d3d9.h>
#include "typedefs.h"
#include "windowhooks.h"
#include "minhook.h"

//globals
CreateWindowExA_t oCreateWindowExA = nullptr;
AdjustWindowRect_t oAdjustWindowRect = nullptr;
AdjustWindowRectEx_t oAdjustWindowRectEx = nullptr;

HWND WINAPI hkCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    if (lpClassName && strcmp(lpClassName, "DeadSpaceWndClass") == 0)
    {
        //strip standard borders
        dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
        dwStyle |= WS_POPUP;

        //strip extended borders
        dwExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

        X = 0; Y = 0;
        nWidth = GetSystemMetrics(SM_CXSCREEN); nHeight = GetSystemMetrics(SM_CYSCREEN);
    }
    return oCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}


//these 2 are also needed for true borderless as the game resizes the window after startup
BOOL WINAPI hkAdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
    dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    return oAdjustWindowRect(lpRect, dwStyle, bMenu);
}


BOOL WINAPI hkAdjustWindowRectEx(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle)
{
    //strips standard & extended borders
    dwStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    dwExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);

    return oAdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
}

void InitialiseWindowHooks()
{
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    LPVOID pCreateWindowExA = GetProcAddress(hUser32, "CreateWindowExA");
    LPVOID pAdjustWindowRect = GetProcAddress(hUser32, "AdjustWindowRect");
    LPVOID pAdjustWindowRectEx = GetProcAddress(hUser32, "AdjustWindowRectEx");

    MH_CreateHook(pCreateWindowExA, &hkCreateWindowExA, reinterpret_cast<LPVOID*>(&oCreateWindowExA));
    MH_CreateHook(pAdjustWindowRect, &hkAdjustWindowRect, reinterpret_cast<LPVOID*>(&oAdjustWindowRect));
    MH_CreateHook(pAdjustWindowRectEx, &hkAdjustWindowRectEx, reinterpret_cast<LPVOID*>(&oAdjustWindowRectEx));

    MH_EnableHook(pCreateWindowExA);
    MH_EnableHook(pAdjustWindowRect);
    MH_EnableHook(pAdjustWindowRectEx);
}