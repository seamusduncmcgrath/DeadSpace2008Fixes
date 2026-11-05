#include "common.h"
#include "Utils.h"
#include "Config.h"
#include "Patches/Patches.h"

#include <functional>
#include <intrin.h>


// A module is a named unit that optionally enables itself from Config, then
// runs its Apply(). dllmain drives them in order, so adding a new fix/patch/
// feature is a single row in the table (and a matching Config group flag).
struct ModuleEntry {
	const char* name;
	bool (*enabled)();
	std::function<void()> apply;
};

void ApplyAllModules()
{
	HMODULE hExe = GetModuleHandleA(nullptr);

	//todo: this method is kinda reaching the end of it's usefulness, getting some race conditions for stuff in here
	const ModuleEntry kModules[] = {
		{ "Patches/System/BorderlessWindow", []() { return Config::Patches::BorderlessWindow; }, &Patches::System::BorderlessWindow::Apply },
		{ "Patches/UI/VersionString",        []() { return true; },                             [hExe]() { Patches::UI::VersionString::Apply(hExe); } },
		{ "Fixes/Graphics/VSync",            []() { return Config::Fixes::VSync; },              [hExe]() { Fixes::Graphics::VSync::Apply(hExe); } },
		{ "Patches/UI/MainIntro",            []() { return Config::Patches::MainIntro; },        [hExe]() { Patches::UI::MainIntro::Apply(hExe); } },
		{ "Patches/Gameplay/UnlockImpossible",[]() {return Config::Patches::UnlockImpossible; }, [hExe]() { Patches::Gameplay::UnlockImpossible::Apply(hExe); } },
		{ "Patches/System/Telemetry",        []() { return Config::Patches::Telemetry; },        &Patches::System::Telemetry::Apply },
		{ "Features/Input/SdlGamepad",       []() { return true; },                             &Features::Input::SdlGamepad::StartThread },
		{ "Fixes/Graphics/SubtitleScale",    []() { return Config::Fixes::SubtitleScale; },      [hExe]() { Fixes::Graphics::SubtitleScale::Apply(hExe); } },
		{ "Patches/Gameplay/IntroCutscene",  []() { return Config::Patches::IntroCutscene; },    [hExe]() { Patches::Gameplay::IntroCutscene::Apply(hExe); } },
		{ "Patches/Gameplay/SkipVideos",     []() { return Config::Patches::SkipVideos; },       [hExe]() { Patches::Gameplay::SkipVideos::Apply(hExe); } },
		{ "Fixes/Save/SafeStringHandling",   []() { return true; },                             [hExe]() { Fixes::Save::SafeStringHandling::Apply(hExe); } },
		{ "Fixes/Input/LegacyDirectInput",   []() { return Config::Fixes::LegacyDirectInput; },  [hExe]() { Input::LegacyDirectInput::Apply(hExe); } },
		{ "Fixes/UI/LoadingScreen",          []() { return Config::Fixes::LoadingScreenDelay; }, [hExe]() { Fixes::UI::LoadingScreen::Apply(hExe); } },
		{ "Features/Graphics/D3D9",          []() { return true; },                             &Features::Graphics::D3D9::InstallDeviceHooks },
		{ "Patches/System/LegacyBorderlessWindow", []() { return Config::Patches::LegacyBorderlessWindow; }, &Patches::System::LegacyBorderlessWindow::Apply },
		{ "Fixes/Input/RemoveWindowsHooks",  []() { return true; },                             [hExe]() { Input::RemoveWindowsHooks::Apply(hExe); } },
		{ "Experimental/Input/MouseLook",    []() { return Config::Fixes::FixMouseSensitivty; },[hExe]() { Input::MouseLook::Apply(hExe); } },
		{ "Features/Input/MenuCamera",       []() { return Config::Features::MainMenuCamera; }, [hExe]() { Input::MenuCamera::Apply(hExe); } },
		{ "Features/Modding/ScriptLoader",   []() { return true; },                             [hExe]() { Features::Modding::ScriptLoader::Apply(hExe); } },
		{ "Features/Modding/SimGroupLoader", []() { return true; }, [hExe]() { Features::Modding::SimGroupLoader::Apply(hExe); } },
	};

	for (const ModuleEntry& m : kModules)
	{
		if (m.enabled())
		{
			LOG_DEBUG("[MainThread]", "Applying %s", m.name);
			m.apply();
		}
	}
}

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

/*
* leaving this here so i remember to finish it at some point
bool SupportsAVX2() {
	int cpuInfo[4] = { 0 };

	__cpuid(cpuInfo, 0);
	int nIds = cpuInfo[0];
	if (nIds < 7)
		return false;

	__cpuidex(cpuInfo, 7, 0);

	return (cpuInfo[1] & (1 << 5)) != 0;
}
*/

DWORD WINAPI MainThread(LPVOID)
{
	//todo: add option to switch to debug mode via flag/config option

#ifdef _DEBUG
	Utils::InitialiseConsole();
#endif

	MH_Initialize();

	HMODULE hExe = GetModuleHandleA(nullptr);

	/*
	* Start applying modules from here now
	if (Config::Features::MainMenuCamera)
		Input::MenuCamera::Apply(hExe);
	*/

	ApplyAllModules();

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		ApplyStartupCPUThreadFix();

		ApplyCpuAffinityFix();

		Config::Load();

		//The startup VSync patch has to run really early or it's broken.
		if (Config::Fixes::VSync)
			Fixes::Graphics::VSync::ApplyStartup();

		CreateThread(nullptr, 0, MainThread, hModule, 0, nullptr);
	}
	else if (ul_reason_for_call == DLL_PROCESS_DETACH)
	{
		Features::Input::SdlGamepad::Shutdown();
	}
	return TRUE;
}
