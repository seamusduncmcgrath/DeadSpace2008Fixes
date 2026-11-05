#include "common.h"
#include "Utils.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace Features {

	namespace Modding {

		namespace ScriptLoader {

			static constexpr const char* kTag = "[Features/Modding/ScriptLoader]";

			const char* const kScriptsFolder = "scripts";

			const int kReloadKey = VK_F5;

			//the game uses lua 5.0.2
			const int kStateTop = 0x08;     //L->top
			const int kStateStack = 0x1C;   //L->stack
			const int kTypeString = 4;      //LUA_TSTRING, a TObject is its type then its value, 8 bytes
			const int kStringHeader = 0x10; //a TString's characters come right after its header

			typedef char(__cdecl* PumpMessages_t)();
			typedef void(__stdcall* CreateGameState_t)(BYTE* pScriptManager);
			typedef void(__cdecl* PrintHandler_t)(const char* pSource, int line, const char* pFunctionName, const char* pText);

			PumpMessages_t oPumpMessages = nullptr;
			CreateGameState_t oCreateGameState = nullptr;

			void* g_pLoadBuffer = nullptr;
			void* g_pProtectedCall = nullptr;
			void* g_pCallFunction = nullptr;

			BYTE** g_ppScriptManager = nullptr; //the gameplay state is at +0x14, the one at +0x18 only runs the upgrade tables

			bool g_RunPending = false;
			bool g_ReloadKeyWasDown = false;

			//the game stubs out the lua print function for some god forsaken reason
			void __cdecl PrintHandler(const char* pSource, int line, const char* /*pFunctionName*/, const char* pText)
			{
				//source is the chunk name, which is @path for files, otherwise it's the chunk's source code itself
				const char* pFile = (pSource && (pSource[0] == '@' || pSource[0] == '=')) ? pSource + 1 : "string";

				//print ends the line with a newline and the log adds its own
				std::string text(pText ? pText : "");
				while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
					text.pop_back();

				if (line > 0)
					LOG_INFO(kTag, "[%s:%d] %s", pFile, line, text.c_str());
				else
					LOG_INFO(kTag, "[%s] %s", pFile, text.c_str());
			}

			BYTE* GetGameState()
			{
				BYTE* pScriptManager = *g_ppScriptManager;
				return pScriptManager ? *reinterpret_cast<BYTE**>(pScriptManager + 0x14) : nullptr;
			}

			//luaL_loadbuffer leaves the compiled chunk on the stack or the error message if it didn't compile
			int LoadBuffer(BYTE* pState, const char* pBuffer, size_t bufferSize, const char* pChunkName)
			{
				int status = 0;
				__asm
				{
					push bufferSize
					push pState
					mov eax, pChunkName
					mov ecx, pBuffer
					call dword ptr [g_pLoadBuffer]
					add esp, 8
					mov status, eax
				}
				return status;
			}

			//lua_pcall(L, 0, 0, 0) the way the game inlines it, runs the function on top of the stack.
			//On an error the function gets swapped for the error message.
			int ProtectedCall(BYTE* pState)
			{
				struct { BYTE* pFunction; int resultCount; } callInfo;
				callInfo.pFunction = *reinterpret_cast<BYTE**>(pState + kStateTop) - 8;
				callInfo.resultCount = 0;

				int functionOffset = static_cast<int>(callInfo.pFunction - *reinterpret_cast<BYTE**>(pState + kStateStack));
				void* pCallInfo = &callInfo;
				int status = 0;
				//jesus how many asm wrappers are there now
				__asm
				{
					push functionOffset
					push pCallInfo
					push g_pCallFunction
					mov esi, pState
					xor edx, edx
					call dword ptr [g_pProtectedCall]
					add esp, 12
					mov status, eax
				}
				return status;
			}

			//what a failed load or call left on top of the stack
			const char* GetErrorMessage(BYTE* pState)
			{
				BYTE* pError = *reinterpret_cast<BYTE**>(pState + kStateTop) - 8;
				if (*reinterpret_cast<int*>(pError) != kTypeString)
					return "(error object is not a string)";

				return *reinterpret_cast<const char**>(pError + 4) + kStringHeader;
			}

			std::string GetScriptsFolder()
			{
				char path[MAX_PATH];
				GetModuleFileNameA(nullptr, path, MAX_PATH);

				std::string exePath(path);
				std::string::size_type pos = exePath.find_last_of("\\/");
				if (pos != std::string::npos)
					return exePath.substr(0, pos) + "\\" + kScriptsFolder;
				return std::string(".\\") + kScriptsFolder;
			}

			//sorted so the load order doesn't depend on the file system, prefix names with 00_, 01_ to order them
			std::vector<std::string> FindScripts(const std::string& folder)
			{
				std::vector<std::string> names;

				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA((folder + "\\*.lua").c_str(), &findData);
				if (hFind == INVALID_HANDLE_VALUE)
					return names;

				do
				{
					//somn bs to get both .luac and .lua
					const char* pExtension = strrchr(findData.cFileName, '.');
					if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && pExtension && _stricmp(pExtension, ".lua") == 0)
						names.push_back(findData.cFileName);
				} while (FindNextFileA(hFind, &findData));
				FindClose(hFind);

				std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
					return _stricmp(a.c_str(), b.c_str()) < 0;
				});
				return names;
			}

			void RunScript(BYTE* pState, const std::string& folder, const std::string& name)
			{
				std::ifstream file(folder + "\\" + name, std::ios::binary);
				if (!file)
				{
					LOG_WARN(kTag, "Couldn't open %s", name.c_str());
					return;
				}
				std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

				//Lua 5.0 dies on the UTF-8 BOM some editors save with (I love old games using even older libraries)
				size_t start = source.compare(0, 3, "\xEF\xBB\xBF") == 0 ? 3 : 0;

				//the @ makes Lua name the file in its errors, scripts\name.lua:line: ...
				std::string chunkName = std::string("@") + kScriptsFolder + "\\" + name;

				//as an offset for the same reason as in ProtectedCall
				ptrdiff_t savedTop = *reinterpret_cast<BYTE**>(pState + kStateTop) - *reinterpret_cast<BYTE**>(pState + kStateStack);

				int status = LoadBuffer(pState, source.data() + start, source.size() - start, chunkName.c_str());
				if (status == 0)
					status = ProtectedCall(pState);

				if (status == 0)
					LOG_INFO(kTag, "Ran %s", name.c_str());
				else
					LOG_WARN(kTag, "Error in %s: %s", name.c_str(), GetErrorMessage(pState));

				//pops the error message if there is one, it only ever shrinks the stack so this is all lua_settop would do
				*reinterpret_cast<BYTE**>(pState + kStateTop) = *reinterpret_cast<BYTE**>(pState + kStateStack) + savedTop;
			}

			void RunScripts(BYTE* pState)
			{
				std::string folder = GetScriptsFolder();
				std::vector<std::string> names = FindScripts(folder);
				LOG_INFO(kTag, "Running %d script(s) from %s", static_cast<int>(names.size()), folder.c_str());

				for (const std::string& name : names)
					RunScript(pState, folder, name);
			}

			bool IsReloadKeyDown()
			{
				if ((GetAsyncKeyState(kReloadKey) & 0x8000) == 0)
					return false;

				DWORD processId = 0;
				GetWindowThreadProcessId(GetForegroundWindow(), &processId);
				return processId == GetCurrentProcessId();
			}

			//World init opens a fresh gameplay state and registers all of the game's functions in it.
			//Run the scripts against it on the next frame, every time the game makes a new one.
			void __stdcall hkCreateGameState(BYTE* pScriptManager)
			{
				oCreateGameState(pScriptManager);
				g_RunPending = true;
			}

			//the game's message pump, called once a frame on the main thread before the game updates, so nothing is part way through Lua
			char __cdecl hkPumpMessages()
			{
				char result = oPumpMessages();

				bool reloadKeyDown = IsReloadKeyDown();
				bool reloadPressed = reloadKeyDown && !g_ReloadKeyWasDown;
				g_ReloadKeyWasDown = reloadKeyDown;

				if (!g_RunPending && !reloadPressed)
					return result;

				BYTE* pState = GetGameState();
				if (!pState)
				{
					if (reloadPressed)
						LOG_INFO(kTag, "The game's Lua isn't up yet, scripts will run once it is");
					return result;
				}

				g_RunPending = false;
				RunScripts(pState);
				return result;
			}

			void Apply(HMODULE hExe)
			{
				uintptr_t printOutput = Utils::FindPattern(hExe, "8B 7C 24 38 85 FF 75 02 8B FE 8B 15 ? ? ? ? 85 D2 8B 74 24 18 74 18");
				if (printOutput)
				{
					PrintHandler_t* pPrintHandler = *reinterpret_cast<PrintHandler_t**>(printOutput + 0xC);
					*pPrintHandler = &PrintHandler;
					LOG_INFO(kTag, "Sending Lua print to the log");
				}
				else
				{
					LOG_WARN(kTag, "Couldn't find the game's print handler, print won't show up");
				}

				//the game's runner for its precompiled UpgradeManager chunk: luaL_loadbuffer, then lua_pcall inlined as luaD_pcall(f_call)
				uintptr_t runChunk = Utils::FindPattern(hExe, "56 8B F0 57 8B 7E 08 2B 7E 0C 68 ? ? ? ? 56 B8 ? ? ? ? B9 ? ? ? ? C1 FF 03 E8 ? ? ? ? 8B D8 83 C4 08 85 DB 75 ? 8B 46 08 83 E8 08 89 44 24 10 2B 46 1C 33 D2 50 8D 44 24 14 50 68 ? ? ? ? C7 44 24 20 FF FF FF FF E8");
				//CallLuaFunction starts with mov eax, [scriptManager] then mov esi, [eax+14h] for the gameplay state
				uintptr_t callLuaFunction = Utils::FindPattern(hExe, "A1 ? ? ? ? 53 8B 5D 0C 56 8B 70 14 8B 43 04 8B 48 0C 57 8B 78 04");
				//opens the gameplay state with lua_open and stores it, mov [ebx+14h], eax
				uintptr_t createGameState = Utils::FindPattern(hExe, "55 8B EC 83 E4 F8 83 EC 0C 53 8B 5D 08 56 57 E8 ? ? ? ? 68 ? ? ? ? 8B F0 89 43 14 E8");
				//PeekMessageA / TranslateMessage / DispatchMessageA until the queue is empty
				uintptr_t pumpMessages = Utils::FindPattern(hExe, "83 EC 1C 53 56 8B 35 ? ? ? ? 6A 03 6A 00 6A 00 6A 00 8D 44 24 18 50 B3 01 FF D6 85 C0 74 3E");

				if (!runChunk || !callLuaFunction || !createGameState || !pumpMessages)
				{
					LOG_WARN(kTag, "Couldn't find everything the script loader needs, not loading scripts");
					return;
				}

				g_pLoadBuffer = reinterpret_cast<void*>(runChunk + 0x22 + *reinterpret_cast<int32_t*>(runChunk + 0x1E));
				g_pCallFunction = *reinterpret_cast<void**>(runChunk + 0x41);
				g_pProtectedCall = reinterpret_cast<void*>(runChunk + 0x52 + *reinterpret_cast<int32_t*>(runChunk + 0x4E));
				g_ppScriptManager = *reinterpret_cast<BYTE***>(callLuaFunction + 1);

				void* pCreateGameState = reinterpret_cast<void*>(createGameState);
				if (MH_CreateHook(pCreateGameState, &hkCreateGameState, reinterpret_cast<void**>(&oCreateGameState)) != MH_OK
					|| MH_EnableHook(pCreateGameState) != MH_OK)
				{
					LOG_WARN(kTag, "Couldn't hook the gameplay Lua state's creation");
					return;
				}

				//in case the game made its state before we got here
				if (GetGameState())
					g_RunPending = true;

				void* pPumpMessages = reinterpret_cast<void*>(pumpMessages);
				if (MH_CreateHook(pPumpMessages, &hkPumpMessages, reinterpret_cast<void**>(&oPumpMessages)) != MH_OK
					|| MH_EnableHook(pPumpMessages) != MH_OK)
				{
					LOG_WARN(kTag, "Couldn't hook the message pump");
					return;
				}

				LOG_INFO(kTag, "Loading scripts from %s, F5 reloads them", GetScriptsFolder().c_str());
			}
		}
	}
}
