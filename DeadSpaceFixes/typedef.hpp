#pragma once
#include <windows.h>
#include <d3d9.h>

typedef HRESULT(__stdcall* SetSamplerState_t)(IDirect3DDevice9*, DWORD, D3DSAMPLERSTATETYPE, DWORD);
SetSamplerState_t oSetSamplerState = nullptr;