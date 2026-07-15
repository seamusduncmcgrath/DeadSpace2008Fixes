#include <Windows.h>
#include <string>
#include <filesystem>
#include "Config.h"

namespace Config {
	bool BorderlessWindowed = true;
	bool PatchOutDInput8 = true;
	bool RemoveTelemetry = true;
	bool SafeFPSCap = false;
	bool FixVSync = true;
	bool FixSubtitleScale = true;
	bool SkipLandingCutscene = false;
	void Load() {
		std::filesystem::path configPath = std::filesystem::current_path() / "DeadSpaceFixes.ini";

		auto configPathStr = configPath.string();

		BorderlessWindowed = GetPrivateProfileIntA("Settings", "BorderlessWindowed", 1, configPathStr.c_str()) != 0;
		PatchOutDInput8 = GetPrivateProfileIntA("Settings", "PatchOutDInput8", 1, configPathStr.c_str()) != 0;
		RemoveTelemetry = GetPrivateProfileIntA("Settings", "RemoveTelemetry", 1, configPathStr.c_str()) != 0;
		FixVSync = GetPrivateProfileIntA("Settings", "FixVSync", 1, configPathStr.c_str()) != 0;
		FixSubtitleScale = GetPrivateProfileIntA("Settings", "FixSubtitleScale", 1, configPathStr.c_str()) != 0;
		SafeFPSCap = GetPrivateProfileIntA("Settings", "SafeFPSCap", 0, configPathStr.c_str()) != 0;
		SkipLandingCutscene = GetPrivateProfileIntA("Settings", "SkipIshimuraLandingCutscene", 0, configPathStr.c_str()) != 0;
	}
}
