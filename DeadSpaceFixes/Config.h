#pragma once

namespace Config {
    extern bool BorderlessWindowed;
    extern bool PatchOutDInput8;
    extern bool RemoveTelemetry;
    extern bool FixVSync;
    extern bool SafeFPSCap;
    extern bool FixSubtitleScale;
    extern bool SkipLandingCutscene;
    extern bool SkipIntroToMainMenu;
    extern bool SkipLoadingScreenDelay;
    void Load();
}
