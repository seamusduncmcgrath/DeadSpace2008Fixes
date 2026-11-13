#include "common.h"
#include "Utils.h"
#include "Config.h"

#include "Shaders/Supersampling.h"

#include <algorithm>
#include <cmath>

namespace Features {

	namespace Graphics {

		namespace Supersampling {

			void* g_pQueueMode = nullptr;
			DWORD __cdecl OnQueueMode(DWORD displayWidth, DWORD displayHeight);

			namespace {

				const char* const kTag = "[Features/Graphics/Supersampling]";

				//the downscale shaders go up to 4x4 taps per back buffer pixel
				const float kMaxScale = 4.0f;

				const char* const kInitDeviceSignature = "8B CE 89 35 ? ? ? ? FF D2 E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? E8 ? ? ? ? C6 05";
				const size_t kInitDeviceCallOffset = 10;

				const char* const kInitFlagsSignature = "C7 44 24 10 54 00 00 00 C7 44 24 14 01 00 00 00 E8";
				const size_t kInitFlagsOffset = 4;
				const char* const kAdapterFlagsSignature = "C7 00 01 00 00 00 33 C0 3B F5 C7 01 54 00 00 00";
				const size_t kAdapterFlagsOffset = 12;
				const BYTE kFlagsWithoutPureDevice = 0x54 & ~D3DCREATE_PUREDEVICE;

				const char* const kQueueModeSignature = "B9 0E 00 00 00 8D 74 24 10 BF ? ? ? ? F3 A5 8B 4D 18 51 8B 4D 0C 53 52 E8";
				const size_t kPresentParamsOffset = 10;
				const size_t kQueueModeCallOffset = 25;

				const char* const kReapplyModeSignature = "0F B7 05 ? ? ? ? 51 0F B7 0D ? ? ? ? 52 50 51 E8 ? ? ? ? A0";
				const size_t kFrameHeightOffset = 3;
				const size_t kFrameWidthOffset = 11;
				const size_t kReapplyModeCallOffset = 18;
				const char* const kRestoreModeSignature = "8B 54 24 1C 52 53 55 57 56 E8 ? ? ? ? 66 A1";
				const size_t kRestoreModeCallOffset = 9;

				const char* const kWindowWidthSignature = "0F B7 05 ? ? ? ? 8B 0D ? ? ? ? 99 F7 F9 66 0F EF C0";
				const size_t kWindowWidthOffset = 0;
				const char* const kWindowHeightSignature = "89 44 24 30 0F B7 05 ? ? ? ? 99 F7 F9";
				const size_t kWindowHeightOffset = 4;

				const char* const kSettingsSignature = "A0 ? ? ? ? 84 C0 74 38 0F B7 0D ? ? ? ? 0F B7 15 ? ? ? ? 89 0D";
				const size_t kSettingsWidthOffset = 9;
				const size_t kSettingsHeightOffset = 16;

				const char* const kCopySignature = "6A 00 6A FF 6A 00 6A 00 51 6A 00 6A 00 52 E8 ? ? ? ? 8B 04 24";
				const size_t kCopyCallOffset = 14;

				//to many damn sigs
				//the device global, same signature as Patches/D3D9Device.cpp
				const char* const kDeviceSignature = "A1 ? ? ? ? 8B 08 8B 51 54 68 ? ? ? ? 6A 00 6A 00 50 FF D2";

				typedef int(__cdecl* InitDevice_t)();
				typedef int(__cdecl* SetMode_t)(int width, int height, int refreshRate, int fullscreen, int vsync);
				typedef HRESULT(WINAPI* LoadSurfaceFromSurface_t)(IDirect3DSurface9* pDestSurface, const PALETTEENTRY* pDestPalette,
					const RECT* pDestRect, IDirect3DSurface9* pSrcSurface, const PALETTEENTRY* pSrcPalette, const RECT* pSrcRect,
					DWORD filter, D3DCOLOR colorKey);

				InitDevice_t g_pInitDevice = nullptr;
				SetMode_t g_pSetMode = nullptr;
				LoadSurfaceFromSurface_t g_pLoadSurfaceFromSurface = nullptr;

				WORD* g_pFrameWidth = nullptr;
				WORD* g_pFrameHeight = nullptr;
				D3DPRESENT_PARAMETERS* g_pPresentParams = nullptr;
				IDirect3DDevice9** g_ppDevice = nullptr;

				float g_Scale = 1.0f;
				//until the device says otherwise
				UINT g_MaxTextureWidth = 8192;
				UINT g_MaxTextureHeight = 8192;

				//frame sizes handed out recently so the frame size the mode switch passes back in maps to the exact
				//display size it came from
				struct SizePair {
					UINT displayWidth;
					UINT displayHeight;
					UINT frameWidth;
					UINT frameHeight;
				};
				SizePair g_RecentSizes[4] = {};
				int g_NextRecentSize = 0;

				//slop rewrite
				// DllMain runs before the console exists, so ApplyStartup records what happened and Apply logs it
				enum class Status { NotRun, Off, Applied, MissingPattern, PatchFailed };
				Status g_Status = Status::NotRun;

				//the downscale
				IDirect3DDevice9* g_pResourceDevice = nullptr;
				IDirect3DVertexShader9* g_pVertexShader = nullptr;
				IDirect3DPixelShader9* g_pPixelShaders[3] = {}; //2, 3 and 4 taps
				IDirect3DVertexDeclaration9* g_pDeclaration = nullptr;
				bool g_DownscaleUnavailable = false;            //then the frame gets stretched into the back buffer instead
				bool g_LoggedDrawFailure = false;
				bool g_LoggedFirstFrame = false;
				D3DFORMAT g_CheckedFrameFormat = D3DFMT_UNKNOWN;
				D3DFORMAT g_CheckedBackBufferFormat = D3DFMT_UNKNOWN;
				bool g_UseSrgb = false;

				//one triangle covering the screen, in clip space
				const float kTriangle[3][2] = { { -1.0f, -1.0f }, { -1.0f, 3.0f }, { 3.0f, -1.0f } };

				//IID_IDirect3DTexture9, spelled out so the project doesn't need dxguid.lib
				const GUID kTextureInterface = { 0x85c31227, 0x3de5, 0x4f00, { 0x9b, 0x3a, 0xf1, 0x1a, 0xc3, 0x8c, 0x18, 0xb5 } };

				const D3DVERTEXELEMENT9 kVertexElements[] = {
					{ 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
					D3DDECL_END()
				};

				uintptr_t CallTarget(uintptr_t call)
				{
					return call + 5 + *reinterpret_cast<int32_t*>(call + 1);
				}

				bool RedirectCall(uintptr_t call, const void* pTarget)
				{
					BYTE bytes[5] = { 0xE8 };
					int32_t offset = static_cast<int32_t>(reinterpret_cast<uintptr_t>(pTarget) - (call + 5));
					memcpy(bytes + 1, &offset, sizeof(offset));
					return Utils::WriteBytes(call, bytes, sizeof(bytes));
				}

				//turns a movzx reg, word [frame size] (7 bytes) into a mov reg, dword [back buffer size] plus a nop
				bool ReadBackBufferSizeInstead(uintptr_t movzx, const void* pBackBufferSize)
				{
					BYTE modrm = *reinterpret_cast<BYTE*>(movzx + 2);
					uintptr_t address = reinterpret_cast<uintptr_t>(pBackBufferSize);
					BYTE bytes[7] = {};
					size_t used = 0;
					if (modrm == 0x05) //eax has its own short form
					{
						bytes[used++] = 0xA1;
					}
					else
					{
						bytes[used++] = 0x8B;
						bytes[used++] = modrm;
					}
					memcpy(bytes + used, &address, sizeof(address));
					used += sizeof(address);
					while (used < sizeof(bytes))
						bytes[used++] = 0x90;
					return Utils::WriteBytes(movzx, bytes, sizeof(bytes));
				}

				void RememberSizes(UINT displayWidth, UINT displayHeight, UINT frameWidth, UINT frameHeight)
				{
					g_RecentSizes[g_NextRecentSize] = { displayWidth, displayHeight, frameWidth, frameHeight };
					g_NextRecentSize = (g_NextRecentSize + 1) % static_cast<int>(sizeof(g_RecentSizes) / sizeof(g_RecentSizes[0]));
				}

				void FrameSizeFor(UINT displayWidth, UINT displayHeight, UINT& frameWidth, UINT& frameHeight)
				{
					if (displayWidth == 0 || displayHeight == 0)
					{
						frameWidth = displayWidth;
						frameHeight = displayHeight;
						return;
					}

					//keep the render targets to a size the GPU can make, at the same aspect ratio
					double scale = g_Scale;
					scale = (std::min)(scale, static_cast<double>(g_MaxTextureWidth) / displayWidth);
					scale = (std::min)(scale, static_cast<double>(g_MaxTextureHeight) / displayHeight);
					scale = (std::max)(scale, 1.0);

					frameWidth = (std::min)(static_cast<UINT>(std::lround(displayWidth * scale)), 0xFFFFu);
					frameHeight = (std::min)(static_cast<UINT>(std::lround(displayHeight * scale)), 0xFFFFu);
					RememberSizes(displayWidth, displayHeight, frameWidth, frameHeight);
				}

				void DisplaySizeFor(UINT frameWidth, UINT frameHeight, UINT& displayWidth, UINT& displayHeight)
				{
					//newest first
					const int count = static_cast<int>(sizeof(g_RecentSizes) / sizeof(g_RecentSizes[0]));
					for (int i = 1; i <= count; i++)
					{
						const SizePair& sizes = g_RecentSizes[(g_NextRecentSize - i + count) % count];
						if (sizes.frameWidth != 0 && sizes.frameWidth == frameWidth && sizes.frameHeight == frameHeight)
						{
							displayWidth = sizes.displayWidth;
							displayHeight = sizes.displayHeight;
							return;
						}
					}

					displayWidth = static_cast<UINT>(std::lround(frameWidth / g_Scale));
					displayHeight = static_cast<UINT>(std::lround(frameHeight / g_Scale));
				}

				//replaces the call to sub_7BFBF0: the device gets created with the display size, then the frame grows
				int __cdecl hkInitDevice()
				{
					int result = g_pInitDevice();

					IDirect3DDevice9* pDevice = *g_ppDevice;
					D3DCAPS9 caps = {};
					if (pDevice && SUCCEEDED(pDevice->GetDeviceCaps(&caps)) && caps.MaxTextureWidth && caps.MaxTextureHeight)
					{
						g_MaxTextureWidth = caps.MaxTextureWidth;
						g_MaxTextureHeight = caps.MaxTextureHeight;
					}

					UINT displayWidth = g_pPresentParams->BackBufferWidth;
					UINT displayHeight = g_pPresentParams->BackBufferHeight;
					UINT frameWidth = 0;
					UINT frameHeight = 0;
					FrameSizeFor(displayWidth, displayHeight, frameWidth, frameHeight);
					*g_pFrameWidth = static_cast<WORD>(frameWidth);
					*g_pFrameHeight = static_cast<WORD>(frameHeight);

					LOG_INFO(kTag, "Rendering at %ux%u for a %ux%u back buffer", frameWidth, frameHeight, displayWidth, displayHeight);
					return result;
				}

				//replaces the mode switchs calls to sub_7BFA00 which pass the frame size
				int __cdecl hkSetModeFromFrameSize(int frameWidth, int frameHeight, int refreshRate, int fullscreen, int vsync)
				{
					UINT displayWidth = 0;
					UINT displayHeight = 0;
					DisplaySizeFor(static_cast<UINT>(frameWidth), static_cast<UINT>(frameHeight), displayWidth, displayHeight);
					return g_pSetMode(static_cast<int>(displayWidth), static_cast<int>(displayHeight), refreshRate, fullscreen, vsync);
				}

				void ReleaseResources()
				{
					if (g_pVertexShader)
						g_pVertexShader->Release();
					g_pVertexShader = nullptr;

					for (IDirect3DPixelShader9*& pShader : g_pPixelShaders)
					{
						if (pShader)
							pShader->Release();
						pShader = nullptr;
					}

					if (g_pDeclaration)
						g_pDeclaration->Release();
					g_pDeclaration = nullptr;

					g_pResourceDevice = nullptr;
				}

				bool CreateResources(IDirect3DDevice9* pDevice)
				{
					if (g_pResourceDevice == pDevice)
						return true;

					ReleaseResources();

					D3DDEVICE_CREATION_PARAMETERS params = {};
					if (SUCCEEDED(pDevice->GetCreationParameters(&params)) && (params.BehaviorFlags & D3DCREATE_PUREDEVICE))
					{
						LOG_WARN(kTag, "The device is still a pure device, so the frame can't be downscaled with a shader, stretching it instead");
						g_DownscaleUnavailable = true;
						return false;
					}

					static const BYTE* const kPixelShaders[3] = { g_SupersamplingPS2, g_SupersamplingPS3, g_SupersamplingPS4 };

					bool created = SUCCEEDED(pDevice->CreateVertexShader(reinterpret_cast<const DWORD*>(g_SupersamplingVS), &g_pVertexShader))
						&& SUCCEEDED(pDevice->CreateVertexDeclaration(kVertexElements, &g_pDeclaration));
					for (int i = 0; created && i < 3; i++)
						created = SUCCEEDED(pDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(kPixelShaders[i]), &g_pPixelShaders[i]));

					if (!created)
					{
						LOG_WARN(kTag, "Couldn't create the downscale shaders, stretching the frame instead");
						ReleaseResources();
						g_DownscaleUnavailable = true;
						return false;
					}

					g_pResourceDevice = pDevice;
					return true;
				}

				//averaging in linear light needs sRGB reads from the frame and sRGB writes to the back buffer
				void CheckSrgb(IDirect3DDevice9* pDevice, D3DFORMAT frameFormat, D3DFORMAT backBufferFormat)
				{
					if (frameFormat == g_CheckedFrameFormat && backBufferFormat == g_CheckedBackBufferFormat)
						return;

					g_CheckedFrameFormat = frameFormat;
					g_CheckedBackBufferFormat = backBufferFormat;
					g_UseSrgb = false;

					IDirect3D9* pD3D = nullptr;
					D3DDEVICE_CREATION_PARAMETERS params = {};
					D3DDISPLAYMODE mode = {};
					if (SUCCEEDED(pDevice->GetDirect3D(&pD3D)) && SUCCEEDED(pDevice->GetCreationParameters(&params))
						&& SUCCEEDED(pD3D->GetAdapterDisplayMode(params.AdapterOrdinal, &mode)))
					{
						g_UseSrgb = SUCCEEDED(pD3D->CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType, mode.Format,
								D3DUSAGE_QUERY_SRGBREAD | D3DUSAGE_QUERY_FILTER, D3DRTYPE_TEXTURE, frameFormat))
							&& SUCCEEDED(pD3D->CheckDeviceFormat(params.AdapterOrdinal, params.DeviceType, mode.Format,
								D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_SRGBWRITE, D3DRTYPE_SURFACE, backBufferFormat));
					}
					if (pD3D)
						pD3D->Release();

					LOG_DEBUG(kTag, "Downscaling in %s", g_UseSrgb ? "linear light" : "gamma space (no sRGB support for these formats)");
				}

				//called once while recording the state block (so it knows what to save) and once for real
				void SetPassState(IDirect3DDevice9* pDevice, IDirect3DTexture9* pFrame, IDirect3DPixelShader9* pPixelShader, const D3DSURFACE_DESC& dest)
				{
					D3DVIEWPORT9 viewport = { 0, 0, dest.Width, dest.Height, 0.0f, 1.0f };
					pDevice->SetViewport(&viewport);

					pDevice->SetVertexDeclaration(g_pDeclaration);
					pDevice->SetVertexShader(g_pVertexShader);
					pDevice->SetPixelShader(pPixelShader);
					const float pixelOffset[4] = { -1.0f / dest.Width, 1.0f / dest.Height, 0.0f, 0.0f };
					const float destPixel[4] = { 1.0f / dest.Width, 1.0f / dest.Height, 0.0f, 0.0f };
					pDevice->SetVertexShaderConstantF(0, pixelOffset, 1);
					pDevice->SetPixelShaderConstantF(0, destPixel, 1);
					//DrawPrimitiveUP clears stream 0, this way the state block puts the games back
					pDevice->SetStreamSource(0, nullptr, 0, 0);

					pDevice->SetTexture(0, pFrame);
					pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
					pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
					pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
					pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
					pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
					pDevice->SetSamplerState(0, D3DSAMP_MAXMIPLEVEL, 0);
					pDevice->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, g_UseSrgb ? TRUE : FALSE);

					pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
					pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
					pDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
					pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
					pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
					pDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
					pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
					pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
					pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
					pDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
					pDevice->SetRenderState(D3DRS_SRGBWRITEENABLE, g_UseSrgb ? TRUE : FALSE);
				}

				bool Downscale(IDirect3DDevice9* pDevice, IDirect3DSurface9* pFrameSurface, const D3DSURFACE_DESC& frame,
					IDirect3DSurface9* pBackBuffer, const D3DSURFACE_DESC& backBuffer)
				{
					if (g_DownscaleUnavailable || !CreateResources(pDevice))
						return false;

					IDirect3DTexture9* pFrame = nullptr;
					if (FAILED(pFrameSurface->GetContainer(kTextureInterface, reinterpret_cast<void**>(&pFrame))))
						return false;

					CheckSrgb(pDevice, frame.Format, backBuffer.Format);

					//enough taps that they're at most a frame pixel apart
					float ratio = (std::max)(static_cast<float>(frame.Width) / backBuffer.Width, static_cast<float>(frame.Height) / backBuffer.Height);
					int taps = (std::min)((std::max)(static_cast<int>(std::ceil(ratio - 0.01f)), 2), 4);
					IDirect3DPixelShader9* pPixelShader = g_pPixelShaders[taps - 2];

					//remember the state the pass changes, so the game finds everything as it left it
					IDirect3DStateBlock9* pSavedState = nullptr;
					if (FAILED(pDevice->BeginStateBlock()))
					{
						pFrame->Release();
						return false;
					}
					SetPassState(pDevice, pFrame, pPixelShader, backBuffer);
					if (FAILED(pDevice->EndStateBlock(&pSavedState)) || FAILED(pSavedState->Capture()))
					{
						if (pSavedState)
							pSavedState->Release();
						pFrame->Release();
						return false;
					}

					//render targets aren't part of state blocks, any extra ones would also have to match the back buffer's size
					IDirect3DSurface9* pSavedTargets[4] = {};
					for (DWORD i = 0; i < 4; i++)
						pDevice->GetRenderTarget(i, &pSavedTargets[i]);

					pDevice->SetRenderTarget(0, pBackBuffer);
					for (DWORD i = 1; i < 4; i++)
					{
						if (pSavedTargets[i])
							pDevice->SetRenderTarget(i, nullptr);
					}

					SetPassState(pDevice, pFrame, pPixelShader, backBuffer);
					//the game ends its scene (sub_7C09A0) before it copies the frame, and D3D9 only draws inside one,
					//so the pass gets a scene of its own (if the game ever is still in one, BeginScene fails and we use that)
					bool ownScene = SUCCEEDED(pDevice->BeginScene());
					HRESULT hr = pDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTriangle, sizeof(kTriangle[0]));
					if (ownScene)
						pDevice->EndScene();

					//setting the targets resets the viewport, which the state block then restores
					for (DWORD i = 0; i < 4; i++)
					{
						if (pSavedTargets[i])
						{
							pDevice->SetRenderTarget(i, pSavedTargets[i]);
							pSavedTargets[i]->Release();
						}
					}
					pSavedState->Apply();
					pSavedState->Release();
					pFrame->Release();

					if (FAILED(hr))
					{
						if (!g_LoggedDrawFailure)
						{
							LOG_WARN(kTag, "Couldn't draw the downscale (0x%08X), stretching the frame instead", static_cast<unsigned int>(hr));
							g_LoggedDrawFailure = true;
						}
						return false;
					}
					return true;
				}

				//replaces the D3DXLoadSurfaceFromSurface call that copies the frame into the back buffer
				HRESULT WINAPI hkCopyFrame(IDirect3DSurface9* pBackBuffer, const PALETTEENTRY* pDestPalette, const RECT* pDestRect,
					IDirect3DSurface9* pFrameSurface, const PALETTEENTRY* pSrcPalette, const RECT* pSrcRect, DWORD filter, D3DCOLOR colorKey)
				{
					D3DSURFACE_DESC frame = {};
					D3DSURFACE_DESC backBuffer = {};
					if (pDestRect || pSrcRect || !pBackBuffer || !pFrameSurface || FAILED(pFrameSurface->GetDesc(&frame))
						|| FAILED(pBackBuffer->GetDesc(&backBuffer)) || (frame.Width == backBuffer.Width && frame.Height == backBuffer.Height))
					{
						return g_pLoadSurfaceFromSurface(pBackBuffer, pDestPalette, pDestRect, pFrameSurface, pSrcPalette, pSrcRect, filter, colorKey);
					}

					IDirect3DDevice9* pDevice = nullptr;
					if (FAILED(pBackBuffer->GetDevice(&pDevice)))
						return g_pLoadSurfaceFromSurface(pBackBuffer, pDestPalette, pDestRect, pFrameSurface, pSrcPalette, pSrcRect, filter, colorKey);

					if (!g_LoggedFirstFrame)
					{
						LOG_INFO(kTag, "First frame: %ux%u downscaled to %ux%u", frame.Width, frame.Height, backBuffer.Width, backBuffer.Height);
						g_LoggedFirstFrame = true;
					}

					HRESULT hr = D3D_OK;
					if (!Downscale(pDevice, pFrameSurface, frame, pBackBuffer, backBuffer))
						hr = pDevice->StretchRect(pFrameSurface, nullptr, pBackBuffer, nullptr, D3DTEXF_LINEAR);

					pDevice->Release();
					return hr;
				}

				//also slop
				// Finds a signature and checks that the instruction at offset is a call
				uintptr_t FindCall(HMODULE hExe, const char* pSignature, size_t offset)
				{
					uintptr_t match = Utils::FindPattern(hExe, pSignature);
					if (!match || *reinterpret_cast<BYTE*>(match + offset) != 0xE8)
						return 0;
					return match + offset;
				}
			}

			//replaces sub_7BF9A0's display size in AX/CX (which it queues as the new frame size) with the frame size
			DWORD __cdecl OnQueueMode(DWORD displayWidth, DWORD displayHeight)
			{
				UINT frameWidth = 0;
				UINT frameHeight = 0;
				FrameSizeFor(displayWidth, displayHeight, frameWidth, frameHeight);
				return (frameHeight << 16) | (frameWidth & 0xFFFF);
			}

			//bruh i have like 20 naked asm hooks now
			__declspec(naked) void QueueModeStub()
			{
				__asm
				{
					pushad
					movzx eax, ax
					movzx ecx, cx
					push ecx
					push eax
					call OnQueueMode
					add esp, 8
					//pushad saved eax at [esp+28] and ecx at [esp+24]
					mov edx, eax
					and eax, 0xFFFF
					shr edx, 16
					mov [esp + 28], eax
					mov [esp + 24], edx
					popad
					jmp dword ptr [g_pQueueMode]
				}
			}

			//gotta be called from dllmain
			void ApplyStartup(float scale)
			{
				if (!(scale > 1.0f))
				{
					g_Status = Status::Off;
					return;
				}
				g_Scale = (std::min)(scale, kMaxScale);

				HMODULE hExe = GetModuleHandleA(nullptr);
				uintptr_t initDeviceCall = FindCall(hExe, kInitDeviceSignature, kInitDeviceCallOffset);
				uintptr_t initFlags = Utils::FindPattern(hExe, kInitFlagsSignature);
				uintptr_t adapterFlags = Utils::FindPattern(hExe, kAdapterFlagsSignature);
				uintptr_t queueModeCall = FindCall(hExe, kQueueModeSignature, kQueueModeCallOffset);
				uintptr_t reapplyModeCall = FindCall(hExe, kReapplyModeSignature, kReapplyModeCallOffset);
				uintptr_t restoreModeCall = FindCall(hExe, kRestoreModeSignature, kRestoreModeCallOffset);
				uintptr_t windowWidth = Utils::FindPattern(hExe, kWindowWidthSignature);
				uintptr_t windowHeight = Utils::FindPattern(hExe, kWindowHeightSignature);
				uintptr_t settings = Utils::FindPattern(hExe, kSettingsSignature);
				uintptr_t copyCall = FindCall(hExe, kCopySignature, kCopyCallOffset);
				uintptr_t device = Utils::FindPattern(hExe, kDeviceSignature);

				//both mode switch calls have to go to the same function
				if (!initDeviceCall || !initFlags || !adapterFlags || !queueModeCall || !reapplyModeCall || !restoreModeCall
					|| !windowWidth || !windowHeight || !settings || !copyCall || !device
					|| CallTarget(reapplyModeCall) != CallTarget(restoreModeCall))
				{
					g_Status = Status::MissingPattern;
					return;
				}

				g_pInitDevice = reinterpret_cast<InitDevice_t>(CallTarget(initDeviceCall));
				g_pSetMode = reinterpret_cast<SetMode_t>(CallTarget(reapplyModeCall));
				g_pQueueMode = reinterpret_cast<void*>(CallTarget(queueModeCall));
				g_pLoadSurfaceFromSurface = reinterpret_cast<LoadSurfaceFromSurface_t>(CallTarget(copyCall));

				uintptr_t reapplyMode = reapplyModeCall - kReapplyModeCallOffset;
				g_pFrameWidth = *reinterpret_cast<WORD**>(reapplyMode + kFrameWidthOffset);
				g_pFrameHeight = *reinterpret_cast<WORD**>(reapplyMode + kFrameHeightOffset);
				g_pPresentParams = *reinterpret_cast<D3DPRESENT_PARAMETERS**>(queueModeCall - kQueueModeCallOffset + kPresentParamsOffset);
				g_ppDevice = *reinterpret_cast<IDirect3DDevice9***>(device + 1);

				const void* pBackBufferWidth = &g_pPresentParams->BackBufferWidth;
				const void* pBackBufferHeight = &g_pPresentParams->BackBufferHeight;

				//the frame copy and the display size fixes first, the frame only grows once the device creation is
				//redirected at the end, so if a write fails partway through the game still renders at the display size
				//zamn
				bool patched = RedirectCall(copyCall, &hkCopyFrame)
					&& ReadBackBufferSizeInstead(windowWidth + kWindowWidthOffset, pBackBufferWidth)
					&& ReadBackBufferSizeInstead(windowHeight + kWindowHeightOffset, pBackBufferHeight)
					&& ReadBackBufferSizeInstead(settings + kSettingsWidthOffset, pBackBufferWidth)
					&& ReadBackBufferSizeInstead(settings + kSettingsHeightOffset, pBackBufferHeight)
					&& Utils::WriteBytes(initFlags + kInitFlagsOffset, &kFlagsWithoutPureDevice, 1)
					&& Utils::WriteBytes(adapterFlags + kAdapterFlagsOffset, &kFlagsWithoutPureDevice, 1)
					&& RedirectCall(reapplyModeCall, &hkSetModeFromFrameSize)
					&& RedirectCall(restoreModeCall, &hkSetModeFromFrameSize)
					&& RedirectCall(queueModeCall, &QueueModeStub)
					&& RedirectCall(initDeviceCall, &hkInitDevice);

				g_Status = patched ? Status::Applied : Status::PatchFailed;
			}

			void Apply(HMODULE)
			{
				if (Config::Features::SSAAScale)
					return;

				switch (g_Status)
				{
				case Status::Applied:
					LOG_INFO(kTag, "Rendering at %.2fx the display resolution and downscaling to it", g_Scale); //big slime
					break;
				case Status::Off:
					LOG_INFO(kTag, "Scale is 1 or less, supersampling is off");
					break;
				case Status::MissingPattern:
					LOG_WARN(kTag, "Couldn't find everything supersampling needs, not applied");
					break;
				case Status::PatchFailed:
					LOG_WARN(kTag, "Couldn't patch everything supersampling needs, it may only be partly applied");
					break;
				case Status::NotRun:
					LOG_WARN(kTag, "ApplyStartup wasn't called from DllMain, supersampling not applied (it has to run before the game creates its device)");
					break;
				}
			}
		}
	}
}
