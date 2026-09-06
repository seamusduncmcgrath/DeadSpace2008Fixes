#pragma once

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
