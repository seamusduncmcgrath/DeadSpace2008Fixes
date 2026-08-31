#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstddef>

// Leveled, tagged logging. Only compiled in _DEBUG builds (the game runs a
// proxy DLL that never ships with this overhead), so the release no-ops are
// valid empty statements to avoid C4390 on `if (x) LOG_INFO(...);`.
//
// Usage:
//   static constexpr const char* kTag = "[Fixes/Physics/Timer]";
//   LOG_DEBUG(kTag, "Found pattern at 0x%X", addr);  // verbose detail
//   LOG_INFO (kTag, "Timer patched");                 // outcome
//   LOG_WARN (kTag, "Failed to hook");                // recoverable problem
//   LOG_ERROR(kTag, "SDL failed to init");            // real error
//
// Default threshold is Info so the noisy "Found <addr>" DEBUG lines don't
// spam; call Utils::SetLogLevel(Utils::LogLevel::Debug) to see everything.
namespace Utils {

	enum class LogLevel { Debug, Info, Warn, Error };

	void InitialiseConsole();
	void SetLogLevel(LogLevel level);
	void Log(LogLevel level, const char* tag, const char* fmt, ...);

	uintptr_t FindPattern(HMODULE hModule, const char* signature);

	uintptr_t FindPattern(HMODULE hModule, const char* signature, uintptr_t startAddress, uintptr_t endAddress);

	uintptr_t FindString(HMODULE hModule, const char* needle);

	bool WriteBytes(uintptr_t address, const void* data, std::size_t size);
}

#ifdef _DEBUG
#define LOG_DEBUG(tag, ...) Utils::Log(Utils::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  Utils::Log(Utils::LogLevel::Info,  tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  Utils::Log(Utils::LogLevel::Warn,  tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Utils::Log(Utils::LogLevel::Error, tag, __VA_ARGS__)
#else
#define LOG_DEBUG(tag, ...) ((void)0)
#define LOG_INFO(tag, ...)  ((void)0)
#define LOG_WARN(tag, ...)  ((void)0)
#define LOG_ERROR(tag, ...) ((void)0)
#endif
