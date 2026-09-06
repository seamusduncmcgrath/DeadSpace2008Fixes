#include <Windows.h>
#include <string>
#include "Config.h"
#include "Utils.h"

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

	//config handling: generates DeadSpaceFixes.ini on first launch
	//(with human-readable help comments above each key), and "heals" an
	//existing file by re-adding any keys that a newer build introduced —
	//complete with their help comments. This replaces the old behaviour of
	//silently reading defaults for unknown keys, so the .ini always stays
	//complete and self-documenting.
	namespace {
		constexpr const char* kIniName = "DeadSpaceFixes.ini";
		// The .ini groups keys into the same categories as the C++ Config
		// namespaces: [Fixes], [Patches] and [Features]. Each known option is
		// declared once here with its section, so the default, the help text and
		// the read/write logic can't drift apart.
		constexpr const char* kFixesSection = "Fixes";
		constexpr const char* kPatchesSection = "Patches";
		constexpr const char* kFeaturesSection = "Features";
		constexpr const char* kInfoSection = "Info";
		constexpr const char* kVersionKey = "ConfigVersion";
		// Pre-v3 builds wrote every key under one flat [Settings] section; the
		// option names never changed, only their grouping. MigrateLegacyConfig
		// carries any user-set values over to the grouped sections.
		constexpr const char* kLegacySection = "Settings";
		// Bump this whenever the config key set changes. A stale config file
		// is then regenerated with the canonical keys instead of silently
		// dropping options the current build doesn't know about.
		constexpr int kConfigVersion = 3;

		// Every known option is declared here once and used everywhere below,
		// so the default, the help text and the read/write logic can't drift
		// apart. "help" becomes a "; comment" line above the key in the .ini
		// (unless nullptr, for keys that are self-explanatory).
		struct Entry {
			const char* section;
			const char* key;
			int def;
			const char* help;
		};

		const Entry kEntries[] = {
			{ kFixesSection,   "FixVSync", 1, "Fix vsync behaviour" },
			{ kFixesSection,   "FixSubtitleScale", 1, "Fix subtitle scaling" },
			{ kFixesSection,   "PatchOutDInput8", 1, "Patch out the game's DInput8 DLL usage" },
			{ kFixesSection,   "UseHighPrecisionTimer", 1, "Use the high-precision timer instead of GetTickCount (helps at high framerates)" },
			{ kFixesSection,   "SkipLoadingScreenDelay", 0, "Skip the artificial wait on the loading screen once the level is ready" },
			{ kPatchesSection, "BorderlessWindowed", 1, "Run the game in a borderless window instead of fullscreen" },
			{ kPatchesSection, "RemoveTelemetry", 1, "Disable the game's telemetry/data collection" },
			{ kPatchesSection, "SkipIshimuraLandingCutscene", 0, "Skip the Ishimura landing cutscene on new game (plus) start" },
			{ kPatchesSection, "SkipIntroToMainMenu", 0, "Skip the boot intro and launch main menu immediately" },
			{ kFeaturesSection, "SafeFPSCap", 0, "Cap the framerate to avoid physics/script issues" },
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
		// Keys are emitted one section at a time, in entry order, so the file
		// reads top-to-bottom as [Fixes] then [Patches] then [Features].
		void WriteDefaultConfig(const std::string& path) {
			WritePrivateProfileStringA(kInfoSection, kVersionKey,
				std::to_string(kConfigVersion).c_str(), path.c_str());

			const char* currentSection = nullptr;
			std::string buffer;
			for (const Entry& e : kEntries) {
				if (e.section != currentSection) {
					if (!buffer.empty()) {
						buffer += '\0';
						WritePrivateProfileSectionA(currentSection, buffer.c_str(), path.c_str());
						buffer.clear();
					}
					currentSection = e.section;
				}
				if (e.help) {
					buffer += "; ";
					buffer += e.help;
					buffer += '\0';
				}
				buffer += e.key;
				buffer += "=";
				buffer += e.def ? "1" : "0";
				buffer += '\0';
			}
			if (!buffer.empty()) {
				buffer += '\0';
				WritePrivateProfileSectionA(currentSection, buffer.c_str(), path.c_str());
			}
		}

		// Self-heal: if a key is absent from an existing .ini, WritePrivateProfile
		// returns the default string we pass in — a sentinel ("\x01") that a real
		// value can never equal. Only when the sentinel comes back do we know the
		// key is genuinely missing and add it with its default.
		void EnsureKeyPresent(const std::string& path, const Entry& e) {
			char value[8] = { 0 };
			constexpr char kMissing[] = "\x01";
			DWORD n = GetPrivateProfileStringA(e.section, e.key, kMissing, value, sizeof(value), path.c_str());
			if (n == 1 && value[0] == kMissing[0])
				WritePrivateProfileStringA(e.section, e.key, e.def ? "1" : "0", path.c_str());
		}

		// The profile API reads and writes key/value pairs but never comments, so
		// comment presence is checked by scanning the raw file text. Used to work
		// out whether a section needs regenerating after migrate/heal added bare
		// keys (below).
		std::string ReadFileText(const std::string& path) {
			HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (h == INVALID_HANDLE_VALUE)
				return std::string();

			std::string text;
			DWORD size = GetFileSize(h, nullptr);
			if (size != INVALID_FILE_SIZE && size > 0) {
				text.resize(size);
				DWORD read = 0;
				if (!ReadFile(h, &text[0], size, &read, nullptr))
					text.clear();
				else
					text.resize(read);
			}
			CloseHandle(h);
			return text;
		}

		// Regenerate one section with a "; help" line above every key, taking the
		// current on-disk values so user-set ones survive. Only called when the
		// section is missing a comment, so a fully self-documenting file (with any
		// manual edits) is left untouched.
		void HealCommentsInSection(const std::string& path, const char* section) {
			std::string buffer;
			for (const Entry& e : kEntries) {
				if (e.section != section)
					continue;
				char value[8] = { 0 };
				GetPrivateProfileStringA(section, e.key, e.def ? "1" : "0", value, sizeof(value), path.c_str());
				if (e.help) {
					buffer += "; ";
					buffer += e.help;
					buffer += '\0';
				}
				buffer += e.key;
				buffer += "=";
				buffer += value;
				buffer += '\0';
			}
			if (!buffer.empty()) {
				buffer += '\0';
				WritePrivateProfileSectionA(section, buffer.c_str(), path.c_str());
			}
		}

		// "Heal comments too": WriteDefaultConfig emits full sections with "; help"
		// lines, but the migrate/heal paths add bare key=value lines via the profile
		// API, so those can end up without their help comment. Whenever any entry of
		// a section is found to be missing its comment, regenerate that section.
		void EnsureCommentsPresent(const std::string& path) {
			std::string text = ReadFileText(path);
			if (text.empty())
				return;

			const char* healed = nullptr;
			for (const Entry& e : kEntries) {
				if (e.help && e.section != healed) {
					std::string needle = "; ";
					needle += e.help;
					if (text.find(needle) == std::string::npos) {
						HealCommentsInSection(path, e.section);
						healed = e.section;
					}
				}
			}
		}

		// Upgrade path for files written by builds older than the grouped
		// sections: the key names are identical, they just all lived in one
		// [Settings] section. Preserve whatever the user set (only touch keys
		// that actually exist in the legacy section), then drop the empty
		// legacy section and stamp the new schema version. Idempotent: a
		// current-format file has no [Settings] section, so it's a no-op.
		void MigrateLegacyConfig(const std::string& path) {
			constexpr char kMissing[] = "\x01";
			bool any = false;
			for (const Entry& e : kEntries) {
				char value[8] = { 0 };
				DWORD n = GetPrivateProfileStringA(kLegacySection, e.key, kMissing, value, sizeof(value), path.c_str());
				if (n != 0 && !(n == 1 && value[0] == kMissing[0])) {
					WritePrivateProfileStringA(e.section, e.key, value, path.c_str());
					any = true;
				}
			}
			if (any) {
				WritePrivateProfileSectionA(kLegacySection, nullptr, path.c_str());
				WritePrivateProfileStringA(kInfoSection, kVersionKey,
					std::to_string(kConfigVersion).c_str(), path.c_str());
				LOG_INFO("[Config]", "Migrated legacy [Settings] config keys to grouped sections");
			}
		}
	}

	void Load() {
		std::string configPath = GetConfigPath();

		// Generate the .ini on first run, otherwise migrate any pre-grouping
		// [Settings] keys and heal keys newer builds added. After either step
		// the file is guaranteed complete in the current format.
		if (GetFileAttributesA(configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			WriteDefaultConfig(configPath);
		}
		else {
			MigrateLegacyConfig(configPath);
			for (const Entry& e : kEntries)
				EnsureKeyPresent(configPath, e);
			EnsureCommentsPresent(configPath);
		}

		auto read = [&](const char* section, const char* key, int def) {
			return GetPrivateProfileIntA(section, key, def, configPath.c_str()) != 0;
		};

		Fixes::VSync = read(kFixesSection, "FixVSync", 1);
		Fixes::SubtitleScale = read(kFixesSection, "FixSubtitleScale", 1);
		Fixes::LegacyDirectInput = read(kFixesSection, "PatchOutDInput8", 1);
		Fixes::LoadingScreenDelay = read(kFixesSection, "SkipLoadingScreenDelay", 0);
		Fixes::HighPrecisionTimer = read(kFixesSection, "UseHighPrecisionTimer", 1);
		Patches::BorderlessWindow = read(kPatchesSection, "BorderlessWindowed", 1);
		Patches::Telemetry = read(kPatchesSection, "RemoveTelemetry", 1);
		Patches::IntroCutscene = read(kPatchesSection, "SkipIshimuraLandingCutscene", 0);
		Patches::MainIntro = read(kPatchesSection, "SkipIntroToMainMenu", 0);
		Features::FrameRateCap = read(kFeaturesSection, "SafeFPSCap", 0);
	}
}
