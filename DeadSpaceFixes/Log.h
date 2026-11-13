#pragma once

#include <Windows.h>

// Leveled, tagged logging. Every build writes DeadSpaceFixes.log next to the game's exe
// (overwritten each launch, flushed after every line so a crash doesn't lose anything).
// Debug builds also print to the console InitialiseConsole opens, and include DEBUG lines.
//
// Usage:
//   static constexpr const char* kTag = "[Fixes/Physics/Timer]";
//   LOG_DEBUG(kTag, "Found pattern at 0x%X", addr);  // verbose detail
//   LOG_INFO (kTag, "Timer patched");                 // outcome
//   LOG_WARN (kTag, "Failed to hook");                // recoverable problem
//   LOG_ERROR(kTag, "SDL failed to init");            // real error
namespace Utils {

	enum class LogLevel { Debug, Info, Warn, Error };

	void InitialiseConsole();
	void SetLogLevel(LogLevel level);
	void Log(LogLevel level, const char* tag, const char* fmt, ...);

	//what's running: the mod's build, the game exe and which version it is, Windows. Call first thing in DllMain.
	void LogStartupInfo(HMODULE hModule);

}

#define LOG_DEBUG(tag, ...) Utils::Log(Utils::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  Utils::Log(Utils::LogLevel::Info,  tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  Utils::Log(Utils::LogLevel::Warn,  tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Utils::Log(Utils::LogLevel::Error, tag, __VA_ARGS__)
