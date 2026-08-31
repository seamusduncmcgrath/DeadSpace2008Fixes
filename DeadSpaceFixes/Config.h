#pragma once

// Configuration settings, grouped by the category of module that consumes them.
// Each C++ identifier matches its on-disk .ini key (see Config.cpp), so a module
// reading one of these reads exactly the setting a user edits. dllmain.cpp
// drives a config -> apply table off these so adding a module is a single entry.

namespace Config {

	namespace Fixes {
		extern bool VSync;              // FixVSync
		extern bool SubtitleScale;      // FixSubtitleScale
		extern bool LegacyDirectInput;  // PatchOutDInput8
		extern bool LoadingScreenDelay; // SkipLoadingScreenDelay
		extern bool HighPrecisionTimer; // UseHighPrecisionTimer
	}

	namespace Patches {
		extern bool BorderlessWindow;  // BorderlessWindowed
		extern bool Telemetry;         // RemoveTelemetry
		extern bool IntroCutscene;     // SkipIshimuraLandingCutscene
		extern bool MainIntro;         // SkipIntroToMainMenu
	}

	namespace Features {
		extern bool FrameRateCap;      // SafeFPSCap
	}

	void Load();
}
