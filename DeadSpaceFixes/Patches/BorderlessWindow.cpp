#include "common.h"
#include "Utils.h"

#include <cstdio>
#include <intrin.h>
#include <vector>

namespace Patches {

	namespace System {

		namespace BorderlessWindow {

			static constexpr const char* Tag = "[Patches/System/BorderlessWindow]";
			const char* const GameWindowClass = "DeadSpaceWndClass";
			const char* const SettingKey = "Window.Borderless";

			const LONG kGameWindowedStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
			const LONG kBorderlessStyle = WS_POPUP | WS_VISIBLE;

			//which part of the options menu is open (the dword at g_pMenuState)
			const int kOptionsMenu = 0;
			const int kVisualsMenu = 4;

#pragma pack(push, 1)
			struct VideoSettings {
				int width;       // Window.Width
				int height;      // Window.Height
				int refreshRate; // Window.Hz
				BYTE fullscreen; // Window.Fullscreen
				BYTE vsync;      // Window.VSync
			};
#pragma pack(pop)

			struct DisplayMode {
				int width;
				int height;
				int refreshRate;
			};

			//the strings a scroll box takes as its options, 16 bytes each
			struct MenuString {
				const char* text;
				uint32_t length;
				uint32_t capacity;
				void* pfnFree;
			};

			typedef int(__stdcall* OnValueChanged_t)(BYTE* pOptions, float value, int arg3);
			typedef void(__stdcall* VisualsRefresh_t)(BYTE* pOptions);
			typedef int(__fastcall* ScrollBoxSetOptions_t)(BYTE* pScrollBox, void* edx, BYTE* pOptionList);
			typedef void(__fastcall* OnSelect_t)(void* ecx, void* edx, BYTE* pOptions, int arg2);
			typedef int(__cdecl* VideoSettingsFn_t)();
			typedef BYTE*(__fastcall* GetFocusedItem_t)(BYTE* pPage, void* edx);

			CreateWindowExA_t oCreateWindowExA = nullptr;
			SetWindowLongA_t oSetWindowLongA = nullptr;
			SetWindowPos_t oSetWindowPos = nullptr;
			WNDPROC oGameWndProc = nullptr;
			OnValueChanged_t oOnValueChanged = nullptr;
			VisualsRefresh_t oVisualsRefresh = nullptr;
			ScrollBoxSetOptions_t oScrollBoxSetOptions = nullptr;
			OnSelect_t oOnSelect = nullptr;
			VideoSettingsFn_t oRevertVideoSettings = nullptr;
			VideoSettingsFn_t oSaveVideoSettings = nullptr;

			GetFocusedItem_t g_pGetFocusedItem = nullptr;
			void* g_pSetMenuValue = nullptr;    //takes the menu page in ESI so we can only call it from asm
			void* g_pReadBoolSetting = nullptr; //these two take the key in EAX, same deal
			void* g_pWriteBoolSetting = nullptr;

			BYTE** g_ppMenuStack = nullptr;
			int* g_pMenuState = nullptr;
			LONG* g_pWindowedStyle = nullptr;
			VideoSettings* g_pSettings = nullptr;
			VideoSettings* g_pSettingsSnapshot = nullptr;

			uintptr_t g_ExeStart = 0;
			uintptr_t g_ExeEnd = 0;

			HWND g_hGameWindow = nullptr;
			//I gotta figure out a better way for this
			//way to many bools
			bool g_Borderless = false;         
			bool g_BorderlessSnapshot = false; 
			bool g_WindowIsBorderless = false; 
			bool g_StyleChanged = false;
			BYTE g_FullscreenBeforeBorderless = 0;
			int g_BorderlessIndex = -1;
			char g_BorderlessLabel[64] = {};

			RECT GetBorderlessRect()
			{
				MONITORINFO info = {};
				info.cbSize = sizeof(info);
				POINT origin = { 0, 0 };
				if (!GetMonitorInfoA(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &info))
				{
					RECT screen = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
					return screen;
				}
				return info.rcMonitor;
			}

			int GetDesktopRefreshRate()
			{
				DEVMODEA mode = {};
				mode.dmSize = sizeof(mode);
				if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1)
					return static_cast<int>(mode.dmDisplayFrequency);
				return 60;
			}

			bool IsGameWindow(HWND hWnd)
			{
				//in case the game made its window before we hooked CreateWindowExA
				if (!g_hGameWindow && hWnd)
				{
					char className[32] = {};
					if (GetClassNameA(hWnd, className, sizeof(className)) && strcmp(className, GameWindowClass) == 0)
						g_hGameWindow = hWnd;
				}
				return hWnd && hWnd == g_hGameWindow;
			}

			//for safety only touch the game's own calls, D3D moves the window around itself when going fullscreen
			bool IsGameCall(void* pReturnAddress)
			{
				uintptr_t address = reinterpret_cast<uintptr_t>(pReturnAddress);
				return address >= g_ExeStart && address < g_ExeEnd;
			}

			//reads/writes a true/false entry in the game's own settings, next to Window.Fullscreen
			bool ReadGameSetting(const char* pKey, bool defaultValue)
			{
				DWORD defaultArg = defaultValue ? 1 : 0;
				BYTE result = 0;
				__asm
				{
					push defaultArg
					mov eax, pKey
					call dword ptr [g_pReadBoolSetting]
					add esp, 4
					mov result, al
				}
				return result != 0;
			}

			void WriteGameSetting(const char* pKey, bool settingValue)
			{
				DWORD valueArg = settingValue ? 1 : 0;
				__asm
				{
					push valueArg
					mov eax, pKey
					call dword ptr [g_pWriteBoolSetting]
					add esp, 4
				}
			}

			//the menu page on top of the menu stack, which is the Visuals page while it's open
			BYTE* GetTopMenuPage()
			{
				BYTE* pMenuStack = *g_ppMenuStack;
				if (!pMenuStack)
					return nullptr;

				uint32_t depth = *reinterpret_cast<uint32_t*>(pMenuStack + 0x38);
				if (depth == 0)
					return nullptr;

				return *reinterpret_cast<BYTE**>(pMenuStack + 0x30 + depth * 0x20);
			}

			//the same lookup the game does at the top of OnValueChanged
			const char* GetFocusedItemName()
			{
				BYTE* pPage = GetTopMenuPage();
				if (!pPage || !pPage[0x4BD])
					return nullptr;

				BYTE* pItem = g_pGetFocusedItem(pPage, nullptr);
				return pItem ? *reinterpret_cast<const char**>(pItem + 0x4C) : nullptr;
			}

			//sets what a menu item shows, 0/1 for a toggle or the index for a list
			void SetMenuValue(const char* pItemName, float menuValue)
			{
				BYTE* pPage = GetTopMenuPage();
				if (!pPage)
					return;

				__asm
				{
					push menuValue
					push pItemName
					mov esi, pPage
					call dword ptr [g_pSetMenuValue]
				}
			}

			//Fullscreen wins over borderless. Our entry isn't a real mode, so point the Resolution
			//list back at one: the same size if there is one, otherwise the biggest.
			void SelectClosestMode(BYTE* pOptions)
			{
				DisplayMode* pModes = *reinterpret_cast<DisplayMode**>(pOptions + 0x150);
				int count = *reinterpret_cast<int*>(pOptions + 0x154);

				int best = -1;
				for (int i = 0; i < count; i++)
				{
					if (pModes[i].width == g_pSettings->width && pModes[i].height == g_pSettings->height
						&& (best == -1 || pModes[i].refreshRate == g_pSettings->refreshRate))
						best = i;
				}
				if (best == -1)
				{
					for (int i = 0; i < count; i++)
					{
						if (best == -1 || pModes[i].width * pModes[i].height > pModes[best].width * pModes[best].height)
							best = i;
					}
				}
				if (best == -1)
					return;

				g_pSettings->width = pModes[best].width;
				g_pSettings->height = pModes[best].height;
				g_pSettings->refreshRate = pModes[best].refreshRate;
				SetMenuValue("Resolution", static_cast<float>(best));
			}

			//runs whenever an options menu item changes value
			int __stdcall hkOnValueChanged(BYTE* pOptions, float value, int arg3)
			{
				if (*g_pMenuState != kVisualsMenu)
					return oOnValueChanged(pOptions, value, arg3);

				const char* name = GetFocusedItemName();
				if (!name)
					return oOnValueChanged(pOptions, value, arg3);

				if (strcmp(name, "Resolution") == 0)
				{
					if (static_cast<int>(value) == g_BorderlessIndex)
					{
						//the game would look our entry up in its mode list and read past the end, so it doesn't get to
						if (!g_Borderless)
							g_FullscreenBeforeBorderless = g_pSettings->fullscreen;

						RECT rc = GetBorderlessRect();
						g_pSettings->width = rc.right - rc.left;
						g_pSettings->height = rc.bottom - rc.top;
						g_pSettings->refreshRate = GetDesktopRefreshRate();
						g_pSettings->fullscreen = 0;
						g_Borderless = true;
						SetMenuValue("FullScreen", 0.0f);
						return 0;
					}

					if (g_Borderless)
					{
						//scrolled off borderless, put FullScreen back how it was
						g_Borderless = false;
						g_pSettings->fullscreen = g_FullscreenBeforeBorderless;
						SetMenuValue("FullScreen", g_FullscreenBeforeBorderless ? 1.0f : 0.0f);
					}
				}
				else if (strcmp(name, "FullScreen") == 0 && value > 0.5f && g_Borderless)
				{
					int result = oOnValueChanged(pOptions, value, arg3);
					g_Borderless = false;
					SelectClosestMode(pOptions);
					return result;
				}

				return oOnValueChanged(pOptions, value, arg3);
			}

			//loads the settings into the Visuals menu, when it opens and after a revert
			void __stdcall hkVisualsRefresh(BYTE* pOptions)
			{
				oVisualsRefresh(pOptions);

				if (g_Borderless && g_pSettings->fullscreen)
					g_Borderless = false;

				if (g_Borderless && g_BorderlessIndex >= 0)
				{
					g_FullscreenBeforeBorderless = 0;
					SetMenuValue("Resolution", static_cast<float>(g_BorderlessIndex));
				}
			}

			//the game fills the Resolution list from its mode list when Visuals opens, add our entry on the end
			int __fastcall hkScrollBoxSetOptions(BYTE* pScrollBox, void* edx, BYTE* pOptionList)
			{
				const char* name = *reinterpret_cast<const char**>(pScrollBox + 0x4C);
				if (!name || strcmp(name, "Resolution") != 0)
					return oScrollBoxSetOptions(pScrollBox, edx, pOptionList);

				MenuString*& pItems = *reinterpret_cast<MenuString**>(pOptionList + 0x2C);
				uint32_t& count = *reinterpret_cast<uint32_t*>(pOptionList + 0x30);

				RECT rc = GetBorderlessRect();
				snprintf(g_BorderlessLabel, sizeof(g_BorderlessLabel), "%d x %d , Borderless", rc.right - rc.left, rc.bottom - rc.top);

				//the scroll box copies the strings, so a temporary list borrowing the game's is fine
				std::vector<MenuString> items(pItems, pItems + count);
				MenuString label = { g_BorderlessLabel, static_cast<uint32_t>(strlen(g_BorderlessLabel)), 0, nullptr };
				items.push_back(label);

				MenuString* pGameItems = pItems;
				uint32_t gameCount = count;
				pItems = items.data();
				count = static_cast<uint32_t>(items.size());

				int result = oScrollBoxSetOptions(pScrollBox, edx, pOptionList);

				pItems = pGameItems;
				count = gameCount;
				g_BorderlessIndex = static_cast<int>(gameCount);
				return result;
			}

			//runs when the player picks something in the options menu, including backing out of Visuals
			void __fastcall hkOnSelect(void* ecx, void* edx, BYTE* pOptions, int arg2)
			{
				int stateBefore = *g_pMenuState;

				//Backing out of Visuals only applies anything if one of the game's own settings changed.
				//If only borderless did, make the snapshot look different for the length of the call so
				//the game does its usual mode switch and prompt.
				bool forceApply = stateBefore == kVisualsMenu && g_Borderless != g_BorderlessSnapshot;
				int snapshotWidth = g_pSettingsSnapshot->width;
				if (forceApply)
					g_pSettingsSnapshot->width = g_pSettings->width + 1;

				oOnSelect(ecx, edx, pOptions, arg2);

				if (forceApply)
					g_pSettingsSnapshot->width = snapshotWidth;

				if (stateBefore == kOptionsMenu && *g_pMenuState == kVisualsMenu)
					g_BorderlessSnapshot = g_Borderless;
			}

			//the "keep these settings?" prompt was turned down or timed out
			int __cdecl hkRevertVideoSettings()
			{
				g_Borderless = g_BorderlessSnapshot;
				return oRevertVideoSettings();
			}

			int __cdecl hkSaveVideoSettings()
			{
				WriteGameSetting(SettingKey, g_Borderless);
				return oSaveVideoSettings();
			}

			HWND WINAPI hkCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
			{
				bool isGameWindow = lpClassName && !IS_INTRESOURCE(lpClassName) && strcmp(lpClassName, GameWindowClass) == 0;

				//only when starting windowed, fullscreen wins
				if (isGameWindow && (dwStyle & WS_CAPTION) == WS_CAPTION)
				{
					//defaults to on so anyone running windowed with the old config-only borderless keeps it
					g_Borderless = ReadGameSetting(SettingKey, true);
					if (g_Borderless)
					{
						RECT rc = GetBorderlessRect();
						dwStyle = static_cast<DWORD>(kBorderlessStyle);
						X = rc.left;
						Y = rc.top;
						nWidth = rc.right - rc.left;
						nHeight = rc.bottom - rc.top;
					}
				}

				HWND hWnd = oCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);

				if (isGameWindow)
				{
					g_hGameWindow = hWnd;
					g_WindowIsBorderless = g_Borderless;
				}
				return hWnd;
			}

			//the game only ever gives its window its windowed style (which has a caption) or its fullscreen one
			LONG WINAPI hkSetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
			{
				if (nIndex == GWL_STYLE && IsGameCall(_ReturnAddress()) && IsGameWindow(hWnd))
				{
					bool windowed = (dwNewLong & WS_CAPTION) == WS_CAPTION;
					g_WindowIsBorderless = windowed && g_Borderless;
					if (g_WindowIsBorderless)
						dwNewLong = kBorderlessStyle;

					g_StyleChanged = true;
				}
				return oSetWindowLongA(hWnd, nIndex, dwNewLong);
			}

			//the game sizes its window to the resolution plus borders without moving it, cover the monitor instead
			BOOL WINAPI hkSetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
			{
				if (IsGameCall(_ReturnAddress()) && IsGameWindow(hWnd))
				{
					if (g_WindowIsBorderless)
					{
						RECT rc = GetBorderlessRect();
						X = rc.left;
						Y = rc.top;
						cx = rc.right - rc.left;
						cy = rc.bottom - rc.top;
						uFlags &= ~(SWP_NOMOVE | SWP_NOSIZE);
					}

					//Windows wants this after a style change before it redraws the frame
					if (g_StyleChanged)
					{
						uFlags |= SWP_FRAMECHANGED;
						g_StyleChanged = false;
					}
				}
				return oSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
			}

			LRESULT CALLBACK hkGameWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
			{
				//The game draws its own cursor and never hides the Windows one, it leaves WM_SETCURSOR to
				//DefWindowProc which puts up the class's arrow. Only exclusive fullscreen kept it hidden, so
				//once you've tabbed out (or are windowed/borderless) the arrow sits on top of the game's
				//cursor. Hide it over the game while the game has focus, the title bar and borders keep theirs.
				if (msg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT && GetForegroundWindow() == hWnd)
				{
					SetCursor(nullptr);
					return TRUE;
				}

				LRESULT result = oGameWndProc(hWnd, msg, wParam, lParam);

				//On WM_SIZE in windowed mode the game saves the window's current style as the one to use
				//next time it goes windowed. While borderless that's our style, which would make picking a
				//normal resolution later come out borderless as well, so put the game's own back.
				if (msg == WM_SIZE && (*g_pWindowedStyle & WS_CAPTION) != WS_CAPTION)
					*g_pWindowedStyle = kGameWindowedStyle;

				return result;
			}

			//todo: switch all hooks to use this helper
			void HookAt(void* pTarget, void* pDetour, void** ppOriginal, const char* name)
			{
				if (!pTarget || MH_CreateHook(pTarget, pDetour, ppOriginal) != MH_OK || MH_EnableHook(pTarget) != MH_OK)
				{
					LOG_WARN(Tag, "Couldn't hook %s", name);
					return;
				}
				LOG_INFO(Tag, "Hooked %s at 0x%X", name, reinterpret_cast<uintptr_t>(pTarget));
			}

			void Apply()
			{
				HMODULE hExe = GetModuleHandleA(nullptr);

				IMAGE_DOS_HEADER* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hExe);
				IMAGE_NT_HEADERS* pNtHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(hExe) + pDosHeader->e_lfanew);
				g_ExeStart = reinterpret_cast<uintptr_t>(hExe);
				g_ExeEnd = g_ExeStart + pNtHeaders->OptionalHeader.SizeOfImage;

				uintptr_t wndProc = Utils::FindPattern(hExe, "55 8B EC 83 E4 F8 83 EC 14 53 8B 5D 0C 83 FB 0F 56");
				//in its WM_SIZE handling: push GWL_STYLE, push hWnd, call GetWindowLongA, mov [windowedStyle], eax
				uintptr_t wmSizeStyle = wndProc ? Utils::FindPattern(hExe, "6A F0 52 FF 15 ? ? ? ? A3 ? ? ? ?", wndProc, wndProc + 0x200) : 0;
				//loads the menu stack, calls GetFocusedItem, takes the name at item+0x4C, then reads the menu state
				uintptr_t onValueChanged = Utils::FindPattern(hExe, "8B 0D ? ? ? ? 8B 41 38 83 EC 0C 53 8B 5C 24 14 56 33 D2 85 C0 57 76 2A C1 E0 05 8B 4C 08 30 33 F6 38 91 BD 04 00 00 74 17 E8 ? ? ? ? 85 C0 74 0E 8B 40 4C 85 C0 8B F0 75 05 BE ? ? ? ? 8B D6 A1 ? ? ? ?");
				uintptr_t visualsRefresh = Utils::FindPattern(hExe, "55 8B EC 83 E4 F8 83 EC 54 53 56 8B 35 ? ? ? ? 33 DB 3B F3 57");
				uintptr_t scrollBoxSetOptions = Utils::FindPattern(hExe, "83 EC 18 53 55 56 8B F1 8D AE A4 02 00 00 55");
				uintptr_t onSelect = Utils::FindPattern(hExe, "55 8B EC 83 E4 F8 81 EC 0C 04 00 00 53 8B 1D ? ? ? ? 56 57 8B 7D 08 83 A7 48 01 00 00 BF");
				//starts loading the snapshot back, mov eax, [snapshot.width] is at +0xE
				uintptr_t revertSettings = Utils::FindPattern(hExe, "51 0F B6 15 ? ? ? ? 8B 0D ? ? ? ? A1 ? ? ? ? 53 0F B6 1D");
				//starts by saving Window.Width, mov eax, [settings.width] is at +6
				uintptr_t saveSettings = Utils::FindPattern(hExe, "55 8B EC 83 E4 F8 A1 ? ? ? ? 50 B8 ? ? ? ? E8");
				uintptr_t readBoolSetting = Utils::FindPattern(hExe, "83 EC 18 53 8A 5C 24 20 55 56 57 8B F8 8D 44 24 17");
				uintptr_t writeBoolSetting = Utils::FindPattern(hExe, "83 EC 10 53 8A 5C 24 18 55 56 57 83 EC 10 8B F8");
				uintptr_t setMenuValue = Utils::FindPattern(hExe, "55 8B AE 2C 02 00 00 57 33 FF 85 ED 0F 86");

				if (!wndProc || !wmSizeStyle || !onValueChanged || !visualsRefresh || !scrollBoxSetOptions || !onSelect
					|| !revertSettings || !saveSettings || !readBoolSetting || !writeBoolSetting || !setMenuValue)
				{
					LOG_WARN(Tag, "Couldn't find everything the Borderless option needs, not hooking anything");
					return;
				}

				g_pWindowedStyle = *reinterpret_cast<LONG**>(wmSizeStyle + 10);
				g_ppMenuStack = *reinterpret_cast<BYTE***>(onValueChanged + 2);
				g_pGetFocusedItem = reinterpret_cast<GetFocusedItem_t>(onValueChanged + 0x2F + *reinterpret_cast<int32_t*>(onValueChanged + 0x2B));
				g_pMenuState = *reinterpret_cast<int**>(onValueChanged + 0x44);
				g_pSettingsSnapshot = *reinterpret_cast<VideoSettings**>(revertSettings + 0xF);
				g_pSettings = *reinterpret_cast<VideoSettings**>(saveSettings + 7);
				g_pReadBoolSetting = reinterpret_cast<void*>(readBoolSetting);
				g_pWriteBoolSetting = reinterpret_cast<void*>(writeBoolSetting);
				g_pSetMenuValue = reinterpret_cast<void*>(setMenuValue);

				HookAt(reinterpret_cast<void*>(wndProc), &hkGameWndProc, reinterpret_cast<void**>(&oGameWndProc), "WndProc");
				HookAt(reinterpret_cast<void*>(onValueChanged), &hkOnValueChanged, reinterpret_cast<void**>(&oOnValueChanged), "OnValueChanged");
				HookAt(reinterpret_cast<void*>(visualsRefresh), &hkVisualsRefresh, reinterpret_cast<void**>(&oVisualsRefresh), "VisualsRefresh");
				HookAt(reinterpret_cast<void*>(scrollBoxSetOptions), &hkScrollBoxSetOptions, reinterpret_cast<void**>(&oScrollBoxSetOptions), "ScrollBoxSetOptions");
				HookAt(reinterpret_cast<void*>(onSelect), &hkOnSelect, reinterpret_cast<void**>(&oOnSelect), "OnSelect");
				HookAt(reinterpret_cast<void*>(revertSettings), &hkRevertVideoSettings, reinterpret_cast<void**>(&oRevertVideoSettings), "RevertVideoSettings");
				HookAt(reinterpret_cast<void*>(saveSettings), &hkSaveVideoSettings, reinterpret_cast<void**>(&oSaveVideoSettings), "SaveVideoSettings");

				HMODULE hUser32 = GetModuleHandleA("user32.dll");
				HookAt(GetProcAddress(hUser32, "CreateWindowExA"), &hkCreateWindowExA, reinterpret_cast<void**>(&oCreateWindowExA), "CreateWindowExA");
				HookAt(GetProcAddress(hUser32, "SetWindowLongA"), &hkSetWindowLongA, reinterpret_cast<void**>(&oSetWindowLongA), "SetWindowLongA");
				HookAt(GetProcAddress(hUser32, "SetWindowPos"), &hkSetWindowPos, reinterpret_cast<void**>(&oSetWindowPos), "SetWindowPos");
			}
		}
	}
}
