#pragma once
#include <Windows.h>
#include <cstdio>
#include <cstddef>

#ifdef _DEBUG
#define DEBUG_LOG(fmt, ...) printf("[DeadSpaceFixes] " fmt "\n", ##__VA_ARGS__)
#else
//#define DEBUG_LOG(fmt, ...) 
#define DEBUG_LOG(fmt, ...) ((void)0) //keeps this a valid non-empty statement, so `if (x) DEBUG_LOG(...);` doesn't trigger C4390 in release builds.
#endif

namespace Utils {
	void InitialiseConsole();

	uintptr_t FindPattern(HMODULE hModule, const char* signature);

	uintptr_t FindPattern(HMODULE hModule, const char* signature, uintptr_t startAddress, uintptr_t endAddress);

	uintptr_t FindString(HMODULE hModule, const char* needle);

	bool WriteBytes(uintptr_t address, const void* data, std::size_t size);
}
