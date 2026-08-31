#include "common.h"
#include "Utils.h"
#include "Config.h"
#include "Fixes/Fixes.h"
#include "Patches/Patches.h"
#include "Features/Features.h"

#include <functional>

namespace {

	// A module is a named unit that optionally enables itself from Config, then
	// runs its Apply(). dllmain drives them in order, so adding a new fix/patch/
	// feature is a single row in the table (and a matching Config group flag).
	struct ModuleEntry {
		const char* name;
		bool (*enabled)();
		std::function<void()> apply;
	};

	// Ordered: fixes and patches first, then the D3D9 device-hook feature last
	// (it waits for the game window and steals the device vtable, so nothing
	// after it can run). All Apply()s that search the main exe share one hExe,
	// resolved here and captured into their thunk.
	void ApplyAllModules()
	{
		HMODULE hExe = GetModuleHandleA(nullptr);

		const ModuleEntry kModules[] = {
			{ "Patches/System/BorderlessWindow", []() { return Config::Patches::BorderlessWindow; }, &Patches::System::BorderlessWindow::Apply },
			{ "Patches/System/Telemetry",        []() { return Config::Patches::Telemetry; },        &Patches::System::Telemetry::Apply },
			{ "Features/Input/SdlGamepad",       []() { return true; },                             &Features::Input::SdlGamepad::StartThread },
			{ "Fixes/Physics/Timer",            []() { return Config::Fixes::HighPrecisionTimer; }, [hExe]() { Fixes::Physics::Timer::Apply(hExe); } },
			{ "Fixes/Graphics/VSync",            []() { return Config::Fixes::VSync; },              [hExe]() { Fixes::Graphics::VSync::Apply(hExe); } },
			{ "Fixes/Graphics/SubtitleScale",    []() { return Config::Fixes::SubtitleScale; },      [hExe]() { Fixes::Graphics::SubtitleScale::Apply(hExe); } },
			{ "Patches/UI/VersionString",        []() { return true; },                             [hExe]() { Patches::UI::VersionString::Apply(hExe); } },
			{ "Patches/Gameplay/IntroCutscene",  []() { return Config::Patches::IntroCutscene; },    [hExe]() { Patches::Gameplay::IntroCutscene::Apply(hExe); } },
			{ "Patches/UI/MainIntro",            []() { return Config::Patches::MainIntro; },        [hExe]() { Patches::UI::MainIntro::Apply(hExe); } },
			{ "Fixes/Save/SafeStringHandling",   []() { return true; },                             [hExe]() { Fixes::Save::SafeStringHandling::Apply(hExe); } },
			{ "Fixes/Input/LegacyDirectInput",   []() { return Config::Fixes::LegacyDirectInput; },  [hExe]() { Fixes::Input::LegacyDirectInput::Apply(hExe); } },
			{ "Fixes/UI/LoadingScreen",          []() { return Config::Fixes::LoadingScreenDelay; }, [hExe]() { Fixes::UI::LoadingScreen::Apply(hExe); } },
			{ "Features/Graphics/D3D9",          []() { return true; },                             &Features::Graphics::D3D9::InstallDeviceHooks },
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
}

DWORD WINAPI MainThread(LPVOID)
{
	//todo: add option to switch to debug mode via flag/config option

#ifdef _DEBUG
	Utils::InitialiseConsole();
#endif

	MH_Initialize();

	ApplyAllModules();

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		//CPU affinity fix: on DS1 if the CPU core/thread count is above 8 it causes
		//crashes, so this caps it to 8. (might work at 10? lots of people say the
		//issue is with 10 or 8)
		DWORD processorCount = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
		LOG_INFO("[MainThread]", "Core/Thread count is %d", processorCount);
		if (processorCount > 8)
		{
			DWORD_PTR affinityMask = 0xFF;
			SetProcessAffinityMask(GetCurrentProcess(), affinityMask);
		}

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
