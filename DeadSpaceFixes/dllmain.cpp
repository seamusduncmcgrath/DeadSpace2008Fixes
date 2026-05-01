#include <windows.h>
#include <d3d9.h>
#include "minhook.h"
#include "typedef.hpp"

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

DWORD WINAPI MainThread(LPVOID)
{
    //wait for the window
    HWND hwnd = nullptr;

    while (!hwnd)
    {
        //could be done better
        hwnd = FindWindowA(nullptr, "Dead Space");
        Sleep(100);
    }

    Sleep(1000);

    if (MH_Initialize() != MH_OK) return 0;

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

    //back to borderless windowed stuff
    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfo(monitor, &mi);

    int width = mi.rcMonitor.right - mi.rcMonitor.left;
    int height = mi.rcMonitor.bottom - mi.rcMonitor.top;


    //removes standard borders
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
    style |= WS_POPUP;
    SetWindowLong(hwnd, GWL_STYLE, style);

    //removes extended borders
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);


    SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, width, height,
        SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}