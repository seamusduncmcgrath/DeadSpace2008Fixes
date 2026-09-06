#pragma once
#include <Windows.h>
#include <cstddef>

#include "Log.h"

namespace Utils {

	uintptr_t FindPattern(HMODULE hModule, const char* signature);

	uintptr_t FindPattern(HMODULE hModule, const char* signature, uintptr_t startAddress, uintptr_t endAddress);

	uintptr_t FindString(HMODULE hModule, const char* needle);

	bool WriteBytes(uintptr_t address, const void* data, std::size_t size);
}
