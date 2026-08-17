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
	bool SkipIntroToMainMenu = false;
	bool SkipLoadingScreenDelay = false;

	//AI-written config handling: generates DeadSpaceFixes.ini on first launch
	//(loading-screen-delay option below reverse engineered by AI, see dllmain.cpp).
	//(with human-readable help comments above each key), and "heals" an
	//existing file by re-adding any keys that a newer build introduced. This
	//replaces the old behaviour of silently reading defaults for unknown keys,
	//so the .ini always stays complete and self-documenting.
	namespace {
		constexpr const char* kIniName = "DeadSpaceFixes.ini";
		constexpr const char* kSettingsSection = "Settings";
		constexpr const char* kInfoSection = "Info";
		constexpr const char* kVersionKey = "ConfigVersion";
		// Bump this whenever the [Settings] key set changes. A stale config file
		// is then regenerated with the canonical keys instead of silently
		// dropping options the current build doesn't know about.
		constexpr int kConfigVersion = 2;

		// Every known option is declared here once and used everywhere below,
		// so the default, the help text and the read/write logic can't drift
		// apart. "help" becomes a "; comment" line above the key in the .ini.
		struct Entry {
			const char* key;
			int def;
			const char* help;
		};

		const Entry kEntries[] = {
			{ "BorderlessWindowed", 1, "Run the game in a borderless window instead of fullscreen" },
			{ "PatchOutDInput8", 1, "Patch out the game's DInput8 DLL usage" },
			{ "RemoveTelemetry", 1, "Disable the game's telemetry/data collection" },
			{ "FixVSync", 1, "Fix vsync behaviour" },
			{ "FixSubtitleScale", 1, "Fix subtitle scaling" },
			{ "SafeFPSCap", 0, "Cap the framerate to avoid physics/script issues" },
			{ "SkipIshimuraLandingCutscene", 0, "Skip the Ishimura landing cutscene on new game (plus) start" },
			{ "SkipIntroToMainMenu", 0, "Skip the boot intro and launch main menu immediately" },
			{ "SkipLoadingScreenDelay", 0, "Skip the artificial wait on the loading screen once the level is ready" },
		};

		// Note: the game's config lives next to the .exe (the DLL runs from the
		// game folder), so resolve it against the working directory rather than
		// %APPDATA%.
		std::string GetConfigPath() {
			return kIniName;
		}

		bool FileExists(const std::string& path) {
			DWORD attributes = GetFileAttributesA(path.c_str());
			return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
		}

		// First run: write a fresh .ini. Each key is preceded by a "; help"
		// line so users know what it does. The [Info] section records the
		// config schema version so a future update can detect stale files.
		void WriteDefaultConfig(const std::string& path) {
			WritePrivateProfileStringA(kInfoSection, kVersionKey,
				std::to_string(kConfigVersion).c_str(), path.c_str());

			std::string section;
			for (const Entry& e : kEntries) {
				section += "; ";
				section += e.help;
				section += '\0';
				section += e.key;
				section += "=";
				section += e.def ? "1" : "0";
				section += '\0';
			}
			section += '\0';
			WritePrivateProfileSectionA(kSettingsSection, section.c_str(), path.c_str());
		}

		// Self-heal: if a key is absent from an existing .ini, WritePrivateProfile
		// returns the default string we pass in — a sentinel ("\x01") that a real
		// value can never equal. Only when the sentinel comes back do we know the
		// key is genuinely missing and add it with its default.
		void EnsureKeyPresent(const std::string& path, const Entry& e) {
			char value[8] = { 0 };
			constexpr char kMissing[] = "\x01";
			DWORD n = GetPrivateProfileStringA(kSettingsSection, e.key, kMissing, value, sizeof(value), path.c_str());
			if (n == 1 && value[0] == kMissing[0])
				WritePrivateProfileStringA(kSettingsSection, e.key, e.def ? "1" : "0", path.c_str());
		}
	}

	void Load() {
		std::string configPath = GetConfigPath();

		// Generate the .ini on first run, otherwise heal any keys that newer
		// builds added. After either step the file is guaranteed complete.
		if (!FileExists(configPath)) {
			WriteDefaultConfig(configPath);
		}
		else {
			for (const Entry& e : kEntries)
				EnsureKeyPresent(configPath, e);
		}

		BorderlessWindowed = GetPrivateProfileIntA(kSettingsSection, "BorderlessWindowed", 1, configPath.c_str()) != 0;
		PatchOutDInput8 = GetPrivateProfileIntA(kSettingsSection, "PatchOutDInput8", 1, configPath.c_str()) != 0;
		RemoveTelemetry = GetPrivateProfileIntA(kSettingsSection, "RemoveTelemetry", 1, configPath.c_str()) != 0;
		FixVSync = GetPrivateProfileIntA(kSettingsSection, "FixVSync", 1, configPath.c_str()) != 0;
		FixSubtitleScale = GetPrivateProfileIntA(kSettingsSection, "FixSubtitleScale", 1, configPath.c_str()) != 0;
		SafeFPSCap = GetPrivateProfileIntA(kSettingsSection, "SafeFPSCap", 0, configPath.c_str()) != 0;
		SkipLandingCutscene = GetPrivateProfileIntA(kSettingsSection, "SkipIshimuraLandingCutscene", 0, configPath.c_str()) != 0;
		SkipIntroToMainMenu = GetPrivateProfileIntA(kSettingsSection, "SkipIntroToMainMenu", 0, configPath.c_str()) != 0;
		SkipLoadingScreenDelay = GetPrivateProfileIntA(kSettingsSection, "SkipLoadingScreenDelay", 0, configPath.c_str()) != 0;
	}
}
