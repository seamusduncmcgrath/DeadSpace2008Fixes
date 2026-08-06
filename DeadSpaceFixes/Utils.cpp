#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <iostream>
#include <vector>
#include <cstdint>

#include "Utils.h"

namespace Utils {
    void InitialiseConsole()
    {
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
    }

    //Converts a signature string like "8B ? 50 F3 0F" into a byte array where -1 means
    //"any byte" (the ? wildcard). Extracted into its own helper so the whole-image and
    //bounded scans below share exactly the same parsing rules (the original FindPattern
    //had this conversion inlined, which would have had to be duplicated for the new
    //bounded variant and could easily have drifted apart).
    static std::vector<int> PatternToBytes(const char* pattern)
    {
        auto bytes = std::vector<int>{};
        auto start = const_cast<char*>(pattern);
        auto end = const_cast<char*>(pattern) + strlen(pattern);

        for (auto current = start; current < end; ++current) {
            if (*current == '?') {
                ++current;
                if (*current == '?') ++current;
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(strtoul(current, &current, 16));
            }
        }
        return bytes;
    }

    //Scans [region, region+regionSize) for patternBytes. Shared by both FindPattern
    //variants. The regionSize <= s guard also fixes a latent issue in the old inlined
    //loop: `sizeOfImage - s` could wrap around to a huge value (and read out of bounds)
    //if a pattern were ever longer than the image being searched.
    static uintptr_t ScanRegion(std::uint8_t* region, std::size_t regionSize, const std::vector<int>& patternBytes)
    {
        auto s = patternBytes.size();
        auto d = patternBytes.data();

        if (regionSize <= s) return 0;

        for (auto i = 0ul; i < regionSize - s; ++i) {
            bool found = true;
            for (auto j = 0ul; j < s; ++j) {
                if (region[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) return reinterpret_cast<uintptr_t>(&region[i]);
        }
        return 0;
    }

    uintptr_t FindPattern(HMODULE hModule, const char* signature)
    {
        auto dosHeader = (PIMAGE_DOS_HEADER)hModule;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)hModule + dosHeader->e_lfanew);

        auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        auto patternBytes = PatternToBytes(signature);

        return ScanRegion(reinterpret_cast<std::uint8_t*>(hModule), sizeOfImage, patternBytes);
    }

    //Bounded variant: searches only [startAddress, endAddress) inside the module image
    //(clamped to the image base/size). Used by SkipIntroToMainMenu, where the attract
    //state's exit sequence is structurally similar to other frontend code and would be
    //ambiguous if searched across the whole ~16 MB image, but is unique when restricted
    //to a short window immediately after its already-found entry point.
    uintptr_t FindPattern(HMODULE hModule, const char* signature, uintptr_t startAddress, uintptr_t endAddress)
    {
        auto base = reinterpret_cast<uintptr_t>(hModule);

        if (startAddress < base) startAddress = base;

        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)hModule + ((PIMAGE_DOS_HEADER)hModule)->e_lfanew);
        auto imageEnd = base + ntHeaders->OptionalHeader.SizeOfImage;
        if (endAddress > imageEnd) endAddress = imageEnd;

        if (endAddress <= startAddress) return 0;

        auto patternBytes = PatternToBytes(signature);
        return ScanRegion(reinterpret_cast<std::uint8_t*>(startAddress), endAddress - startAddress, patternBytes);
    }

    bool WriteBytes(uintptr_t address, const void* data, std::size_t size)
    {
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        memcpy(reinterpret_cast<void*>(address), data, size);
        VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        return true;
    }
}
