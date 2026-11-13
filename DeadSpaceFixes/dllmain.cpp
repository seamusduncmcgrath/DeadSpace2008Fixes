#include "common.h"
#include "Utils.h"
#include "Config.h"
#include "Patches/Patches.h"

void ApplyStartupCPUThreadFix()
{
	HMODULE hExe = GetModuleHandleA(nullptr);
	const BYTE skipCall[6] = { 0x83, 0xC4, 0x08, 0x90, 0x90, 0x90 }; //add esp, 8 (drop hThread + mask)

	if (uintptr_t main = Utils::FindPattern(hExe, "BA 01 00 00 00 D3 E2 83 C4 04 52 FF 15 ? ? ? ? 50 FF 15 ? ? ? ? A1"))
		Utils::WriteBytes(main + 18, skipCall, sizeof(skipCall));

	if (uintptr_t render = Utils::FindPattern(hExe, "BD 01 00 00 00 8B C5 56 D3 E0 57 50 FF 15 ? ? ? ? 50 FF 15 ? ? ? ? 8B 74 24 14"))
		Utils::WriteBytes(render + 19, skipCall, sizeof(skipCall));
}

void ApplyCpuAffinityFix()
{
	DWORD processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
	LOG_INFO("[MainThread]", "Core/Thread count is %d", processorCount);
	if (processorCount > 8)
	{
		DWORD_PTR affinityMask = 0xFF;
		SetProcessAffinityMask(GetCurrentProcess(), affinityMask);
	}
}

void ApplyPatches()
{
#ifdef _DEBUG
	Utils::InitialiseConsole();
#endif

	MH_Initialize();

	HMODULE hExe = GetModuleHandleA(nullptr);

	Patches::System::BorderlessWindow::Apply();

	Patches::UI::VersionString::Apply(hExe);

	Fixes::Graphics::VSync::Apply(hExe);

	Patches::UI::MainIntro::Apply(hExe);

	Patches::Gameplay::UnlockImpossible::Apply(hExe);

	Fixes::Save::SafeStringHandling::Apply(hExe);

	Features::Input::SdlGamepad::StartThread();

	Features::Graphics::Supersampling::Apply(hExe);

	Fixes::Physics::Timer::Apply(hExe);

	Fixes::Physics::SolverDeactivation::Apply(hExe);

	Patches::System::Telemetry::Apply();

	Patches::Gameplay::SkipVideos::Apply(hExe);

	Patches::Gameplay::IntroCutscene::Apply(hExe);

	Fixes::Graphics::SubtitleScale::Apply(hExe);

	Features::Graphics::D3D9On12::Apply(hExe);

	CreateThread(nullptr, 0, [](LPVOID) -> DWORD { Features::Graphics::D3D9::InstallDeviceHooks(); return 0; }, nullptr, 0, nullptr); //lowkey ignore this
	//Features::Graphics::D3D9::InstallDeviceHooks();

	Features::Graphics::FXAA::Apply(hExe); //do config

	Input::LegacyDirectInput::Apply(hExe);

	Fixes::UI::LoadingScreen::Apply(hExe);

	Input::MenuCamera::Apply(hExe);

	Input::RemoveWindowsHooks::Apply(hExe);

	Input::MouseLook::Apply(hExe);

	Patches::System::LegacyBorderlessWindow::Apply();

	Features::Modding::ScriptLoader::Apply(hExe);
	Features::Modding::SimGroupLoader::Apply(hExe);
}

const char* const kWinMainCallSignature = "84 5D BC 74 06 0F B7 4D C0 EB 03 6A 0A 59 51 50 6A 00 68 00 00 40 00 E8 ? ? ? ? 89 45 E4";
WinMain_t pGameWinMain = nullptr;

int WINAPI hkWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	LARGE_INTEGER frequency, start, end;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);

	ApplyPatches();

	QueryPerformanceCounter(&end);
	double ms = (end.QuadPart - start.QuadPart) * 1000.0 / frequency.QuadPart;
	LOG_INFO("[WinMain]", "Applied the patches in %.1f ms before the game started", ms);

	return pGameWinMain(hInstance, hPrevInstance, lpCmdLine, nShowCmd);
}

bool WinMainRedirect(HMODULE hExe)
{
	uintptr_t winMainAddr = Utils::FindPattern(hExe, kWinMainCallSignature);
	if (!winMainAddr)
		return false;

	uintptr_t call = winMainAddr + 23;

	int32_t offset = *reinterpret_cast<int32_t*>(call + 1);
	pGameWinMain = reinterpret_cast<WinMain_t>(call + 5 + offset);

	int32_t newOffset = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&hkWinMain)) - (call + 5); //lowkey just ignore this (quitely)

	return Utils::WriteBytes(call + 1, &newOffset, sizeof(newOffset));
}

DWORD WINAPI MainThread(LPVOID)
{
	ApplyPatches();
	LOG_WARN("Important!", "Failed to redirect WinMain!");
	return 0;
}

//everything that has to happen before the game's own code starts running
void ApplyStartupPatches()
{
	ApplyStartupCPUThreadFix();

	//The startup VSync patch has to run really early or it's broken.
	if (Config::Fixes::VSync)
		Fixes::Graphics::VSync::ApplyStartup();

	//has to run before the game creates its shaders
	Features::Graphics::FXAA::ApplyStartup();

	Features::Graphics::Supersampling::ApplyStartup(Config::Features::SSAAScale);

	if (!WinMainRedirect(GetModuleHandleA(nullptr)))
		CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
}

//the steam build uses SteamStub DRM to encrypt the code so we gotta wait till it's been decrypted
GetStartupInfoA_t oGetStartupInfoA = nullptr;
void** g_pStartupInfoImport = nullptr;

inline bool GameCodeIsReadable()
{
	return Utils::FindPattern(GetModuleHandleA(nullptr), kWinMainCallSignature) != 0;
}

VOID WINAPI hkGetStartupInfoA(LPSTARTUPINFOA pInfo)
{
	oGetStartupInfoA(pInfo);

	//already done, or something calling it before the game has been decrypted
	if (!g_pStartupInfoImport || !GameCodeIsReadable())
		return;

	Utils::WriteBytes(reinterpret_cast<uintptr_t>(g_pStartupInfoImport), &oGetStartupInfoA, sizeof(oGetStartupInfoA));
	g_pStartupInfoImport = nullptr;

	LOG_INFO("[Startup]", "The game's code has been decrypted, patching now");
	ApplyStartupPatches();
}

//the slot in the exe's import table that Windows filled in with pFunction
void** FindImportSlot(HMODULE hExe, void* pFunction)
{
	BYTE* pBase = reinterpret_cast<BYTE*>(hExe);
	auto pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(pBase + reinterpret_cast<IMAGE_DOS_HEADER*>(pBase)->e_lfanew);
	const IMAGE_DATA_DIRECTORY& imports = pNt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!imports.VirtualAddress || !pFunction)
		return nullptr;

	for (auto pDescriptor = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pBase + imports.VirtualAddress); pDescriptor->Name; pDescriptor++)
	{
		for (auto pThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(pBase + pDescriptor->FirstThunk); pThunk->u1.Function; pThunk++)
		{
			if (reinterpret_cast<void*>(pThunk->u1.Function) == pFunction)
				return reinterpret_cast<void**>(&pThunk->u1.Function);
		}
	}
	return nullptr;
}

//last resort for if the code is still encrypted
DWORD WINAPI WaitForGameCodeThread(LPVOID)
{
	for (int i = 0; i < 2000 && !GameCodeIsReadable(); i++)
		Sleep(5);

	LOG_WARN("[Startup]", "Patching from a thread after waiting for the game's code, some patches may be too late");
	ApplyStartupPatches();
	return 0;
}

void WaitForGameCode()
{
	HMODULE hExe = GetModuleHandleA(nullptr);

	//GetProcAddress follows kernel32s forward to kernelbase the same way Windows did when it filled in the import
	oGetStartupInfoA = reinterpret_cast<GetStartupInfoA_t>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetStartupInfoA"));
	g_pStartupInfoImport = FindImportSlot(hExe, reinterpret_cast<void*>(oGetStartupInfoA));

	void* pHook = reinterpret_cast<void*>(&hkGetStartupInfoA);
	if (g_pStartupInfoImport && Utils::WriteBytes(reinterpret_cast<uintptr_t>(g_pStartupInfoImport), &pHook, sizeof(pHook)))
	{
		LOG_INFO("[Startup]", "The game's code is still encrypted (Steam DRM), waiting for the game to start before patching");
		return;
	}

	g_pStartupInfoImport = nullptr;
	LOG_WARN("[Startup]", "The game's code is still encrypted and GetStartupInfoA's import wasn't found");
	CreateThread(nullptr, 0, WaitForGameCodeThread, nullptr, 0, nullptr);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		Utils::LogStartupInfo(hModule);

		ApplyCpuAffinityFix();

		Config::Load();

		if (GameCodeIsReadable())
			ApplyStartupPatches();
		else
			WaitForGameCode();
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		Features::Input::SdlGamepad::Shutdown();
	}
	return TRUE;
}
