#include <Windows.h>
#include <string>
#include "Config.h"

namespace Config {

	namespace Fixes {
		bool VSync = true;
		bool SubtitleScale = true;
		bool LegacyDirectInput = true;
		bool LoadingScreenDelay = false;
		bool HighPrecisionTimer = true;
	}

	namespace Patches {
		bool BorderlessWindow = true;
		bool Telemetry = true;
		bool IntroCutscene = false;
		bool MainIntro = false;
	}

	namespace Features {
		bool FrameRateCap = false;
	}

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
			{ "UseHighPrecisionTimer", 1, "Use the high-precision timer instead of GetTickCount (helps at high framerates)" },
		};

		// Note: the game's config lives next to the .exe (the DLL runs from the
		// game folder), so resolve it against the working directory rather than
		// %APPDATA%.
		std::string GetConfigPath() {
			char path[MAX_PATH];
			GetModuleFileNameA(nullptr, path, MAX_PATH);

			std::string fullPath(path);
			std::string::size_type pos = fullPath.find_last_of("\\/");
			if (pos != std::string::npos) {
				return fullPath.substr(0, pos) + "\\" + kIniName;
			}
			return std::string(".\\") + kIniName;
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
		if (GetFileAttributesA(configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			WriteDefaultConfig(configPath);
		}
		else {
			for (const Entry& e : kEntries)
				EnsureKeyPresent(configPath, e);
		}

		auto read = [&](const char* key, int def) {
			return GetPrivateProfileIntA(kSettingsSection, key, def, configPath.c_str()) != 0;
		};

		Fixes::VSync = read("FixVSync", 1);
		Fixes::SubtitleScale = read("FixSubtitleScale", 1);
		Fixes::LegacyDirectInput = read("PatchOutDInput8", 1);
		Fixes::LoadingScreenDelay = read("SkipLoadingScreenDelay", 0);
		Fixes::HighPrecisionTimer = read("UseHighPrecisionTimer", 1);
		Patches::BorderlessWindow = read("BorderlessWindowed", 1);
		Patches::Telemetry = read("RemoveTelemetry", 1);
		Patches::IntroCutscene = read("SkipIshimuraLandingCutscene", 0);
		Patches::MainIntro = read("SkipIntroToMainMenu", 0);
		Features::FrameRateCap = read("SafeFPSCap", 0);
	}
}
