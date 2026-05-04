#pragma once
#include <Windows.h>

#ifdef _DEBUG
#define DEBUG_LOG(fmt, ...) printf("[DeadSpaceFixes] " fmt "\n", ##__VA_ARGS__)
#else
#define DEBUG_LOG(fmt, ...) 
#endif

void InitialiseConsole();

uintptr_t FindPattern(HMODULE hModule, const char* signature);