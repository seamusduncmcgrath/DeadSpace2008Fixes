#pragma once

namespace Config {

	namespace Fixes {
		extern bool VSync;              // FixVSync
		extern bool SubtitleScale;      // FixSubtitleScale
		extern bool LegacyDirectInput;  // PatchOutDInput8
		extern bool LoadingScreenDelay; // SkipLoadingScreenDelay
		extern bool FixMouseSensitivty;
	}

	namespace Patches {
		extern bool BorderlessWindow;  // BorderlessWindowed
		extern bool Telemetry;         // RemoveTelemetry
		extern bool IntroCutscene;     // SkipIshimuraLandingCutscene
		extern bool MainIntro;         // SkipIntroToMainMenu
		extern bool SkipVideos;        // SkipLoreVideos
		extern bool LegacyBorderlessWindow;
		extern bool UnlockImpossible;
	}
	
	namespace Features {
		extern bool FrameRateCap;      // SafeFPSCap
		extern bool MainMenuCamera;
		extern float ADSMouseSensitivity;
	}

	void Load();
}
