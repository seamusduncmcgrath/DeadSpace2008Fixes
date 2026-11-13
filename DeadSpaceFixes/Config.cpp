#include <Windows.h>
#include <string>
#include <charconv>
#include <cmath>
#include <cstring>
#include "Config.h"
#include "Utils.h"

namespace Config {

	namespace Fixes {
		bool VSync = true;
		bool SubtitleScale = true;
		bool LegacyDirectInput = true;
		bool LoadingScreenDelay = false;
		bool FixMouseSensitivty = true;
		bool HavokSolverFix = true;
		bool GameTimerFix = true;
	}

	namespace Patches {
		bool BorderlessWindow = true;
		bool Telemetry = true;
		bool IntroCutscene = false;
		bool MainIntro = false;
		bool SkipVideos = false;
		bool LegacyBorderlessWindow = false;
		bool UnlockImpossible = false;
	}

	namespace Features {
		bool FrameRateCap = false;
		bool MainMenuCamera = false;
		float ADSMouseSensitivity = 1.0f;
		bool D3D9On12 = false;
		bool UseFXAA = true;
		float SSAAScale = 1.0;
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
		constexpr char kFixesSection[] = "Fixes";
		constexpr char kPatchesSection[] = "Patches";
		constexpr char kFeaturesSection[] = "Features";
		constexpr char kInfoSection[] = "Info";
		constexpr char kVersionKey[] = "ConfigVersion";
		// Pre-v3 builds wrote every key under one flat [Settings] section; the
		// option names never changed, only their grouping. MigrateLegacyConfig
		// carries any user-set values over to the grouped sections.
		constexpr char kLegacySection[] = "Settings";
		// Bump this whenever the config key set changes. A stale config file
		// is then regenerated with the canonical keys instead of silently
		// dropping options the current build doesn't know about.
		constexpr int kConfigVersion = 9;

		// Every known option is declared here once and used everywhere below,
		// so the default, the help text and the read/write logic can't drift
		// apart. "help" becomes a "; comment" line above the key in the .ini
		// (unless nullptr, for keys that are self-explanatory).
		struct Entry {
			const char* section;
			const char* key;
			const char* def;
			const char* help;
		};

		const Entry kEntries[] = {
			{ kFixesSection,   "FixVSync", "1", "Fix vsync behaviour" },
			{ kFixesSection,   "FixSubtitleScale", "1", "Fix subtitle scaling" },
			{ kFixesSection,   "PatchOutDInput8", "1", "Patch out the game's DInput8 DLL usage" },
			{ kFixesSection,   "SkipLoadingScreenDelay", "0", "Skip the artificial wait on the loading screen once the level is ready" },
			{ kFixesSection,   "FixMouseSensitivty", "1", "Makes it so the mouse sensitivty is no longer tied to the framerate" },
			{ kFixesSection,   "HavokSolverFix", "1", "Fixes the havok solver deactivation at high FPS. This fixes several issues, most notable to immovable chapter 10 bunks and other kinesis stuff" },
			{ kFixesSection,   "GameTimerFix", "1", "Fixes the game's timer to prevent the game from slowing down at high FPS and fixes several game breaking bugs as a result of that" },
			{ kPatchesSection, "BorderlessWindowed", "1", "Adds a borderless option in the games resolution list" },
			{ kPatchesSection, "LegacyBorderlessWindowed", "0", "Run the game in a borderless window instead of fullscreen (legacy as there's a chance of the main borderless patch causing issues when switching resolution)" },
			{ kPatchesSection, "RemoveTelemetry", "0", "Disable the game's telemetry/data collection" },
			{ kPatchesSection, "SkipIshimuraLandingCutscene", "0", "Skip the Ishimura landing cutscene on new game (plus) start" },
			{ kPatchesSection, "SkipIntroToMainMenu", "1", "Skip the boot intro and launch main menu immediately" },
			{ kPatchesSection, "SkipLoreVideos", "0", "Make newly-triggered lore/vidlog videos skippable with F" },
			{ kPatchesSection, "UnlockImpossible", "0", "Unlocks impossible difficulty without having to play through the whole game" },
			{ kFeaturesSection, "SafeFPSCap", "0", "Cap the framerate to avoid physics/script issues" },
			{ kFeaturesSection, "MouseMenuCamera", "0", "Allows you to move the main menu camera with a mouse, normally you have to use a controller." },
			{ kFeaturesSection, "ADSMouseSensitivity", "1.0", "Changes the aim down sights sensitivity when FixMouseSensitivty is enabled. (only applys when FixMouseSensitivty=1)" },
			{ kFeaturesSection, "UseFXAA", "1", "Swaps the game's basic anti-aliasing for FXAA 3.1 (will look a bit more blurry but deals with aliasing far better" },
			{ kFeaturesSection, "SSAAScale", "1.0", "Controls the scale of the screen space anti-aliasing, which removes almost all aliasing but at a high performance cost. A value of 1.0 disables it"},
			{ kFeaturesSection, "D3D9On12", "0", "Enabled D3D9On12 which is Microsoft's D3D9 driver implemented with D3D12, it can fix some issues on new GPUs but will reduce performance (requires a newish version of Windows 10 or 11)" }, };

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
				buffer += e.def;
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
			char value[32] = { 0 };
			constexpr char kMissing[] = "\x01";
			DWORD n = GetPrivateProfileStringA(e.section, e.key, kMissing, value, sizeof(value), path.c_str());
			if (n == 1 && value[0] == kMissing[0])
				WritePrivateProfileStringA(e.section, e.key, e.def, path.c_str());
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
				char value[32] = { 0 };
				GetPrivateProfileStringA(section, e.key, e.def, value, sizeof(value), path.c_str());
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
				char value[32] = { 0 };
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

		//lord why is there no GetPrivateProfileFloatA
		auto readFloat = [&](const char* section, const char* key, float def, float min, float max) {
			char buffer[32] = {};
			GetPrivateProfileStringA(section, key, "", buffer, sizeof(buffer), configPath.c_str());

			float value = def;
			//stops at the first character that isn't part of a number, so "0.15 ; comment" still reads as 0.15
			auto [ptr, ec] = std::from_chars(buffer, buffer + strlen(buffer), value);
			if (ec != std::errc() || ptr == buffer || !std::isfinite(value))
				return def;

			return value < min ? min : (value > max ? max : value);
			};


		Fixes::VSync = read(kFixesSection, "FixVSync", 1);
		Fixes::SubtitleScale = read(kFixesSection, "FixSubtitleScale", 1);
		Fixes::LegacyDirectInput = read(kFixesSection, "PatchOutDInput8", 1);
		Fixes::LoadingScreenDelay = read(kFixesSection, "SkipLoadingScreenDelay", 0);
		Fixes::FixMouseSensitivty = read(kFixesSection, "FixMouseSensitivty", 1);
		Fixes::HavokSolverFix = read(kFixesSection, "HavokSolverFix", 1);
		Fixes::GameTimerFix = read(kFixesSection, "GameTimerFix", 1);
		Patches::BorderlessWindow = read(kPatchesSection, "BorderlessWindowed", 1);
		Patches::LegacyBorderlessWindow = read(kPatchesSection, "LegacyBorderlessWindowed", 0);
		Patches::Telemetry = read(kPatchesSection, "RemoveTelemetry", 1);
		Patches::IntroCutscene = read(kPatchesSection, "SkipIshimuraLandingCutscene", 0);
		Patches::MainIntro = read(kPatchesSection, "SkipIntroToMainMenu", 0);
		Patches::SkipVideos = read(kPatchesSection, "SkipLoreVideos", 0);
		Patches::UnlockImpossible = read(kPatchesSection, "UnlockImpossible", 0);
		Features::FrameRateCap = read(kFeaturesSection, "SafeFPSCap", 0);
		Features::MainMenuCamera = read(kFeaturesSection, "MouseMenuCamera", 0);
		Features::ADSMouseSensitivity = readFloat(kFeaturesSection, "ADSMouseSensitivity", 1.0f, 0.01f, 10.0f);
		Features::UseFXAA = read(kFeaturesSection, "UseFXAA", 1);
		Features::SSAAScale = readFloat(kFeaturesSection, "SSAAScale", 1.0, 1.0, 4.0);
		Features::D3D9On12 = read(kFeaturesSection, "D3D9On12", 0);
	}
}
