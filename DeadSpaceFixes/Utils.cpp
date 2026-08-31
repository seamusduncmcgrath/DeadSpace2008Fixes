#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdarg>
#include <emmintrin.h>
#include <intrin.h>

#include "Utils.h"

namespace Utils {
    void InitialiseConsole()
    {
        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
    }

    namespace {
        // Only messages at or above this level are printed. Defaults to Info so
        // the verbose "Found <addr>" DEBUG lines stay out of the way.
        LogLevel g_LogLevel = LogLevel::Info;
    }

    void SetLogLevel(LogLevel level)
    {
        g_LogLevel = level;
    }

    void Log(LogLevel level, const char* /*tag*/, const char* fmt, ...)
    {
        if (level < g_LogLevel)
            return;

        static const char* kLevelNames[] = { "DBG", "INF", "WRN", "ERR" };
        const char* levelNames = (level >= LogLevel::Debug && level <= LogLevel::Error)
            ? kLevelNames[static_cast<int>(level)] : "???";

        printf("[DeadSpaceFixes] [%s] ", levelNames);

        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
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

        for (std::size_t i = 0ul; i <= regionSize - s; ++i) {
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

    //rewrite of ScanRegion to use SSE2 cause why not, just about every 64bit CPU since 2003 supports it, and this is like 16x faster
    static uintptr_t ScanRegionSSE2(std::uint8_t* region, std::size_t regionSize, const std::vector<int>& patternBytes) {
        const std::size_t patternLen = patternBytes.size();
        if (regionSize < patternLen) return 0;

        //find the first non wildcarded byte to use as a needle
        std::size_t firstValidIdx = 0;
        for (; firstValidIdx < patternLen; ++firstValidIdx) 
        {
            if (patternBytes[firstValidIdx] != -1) break;
        }

        if (firstValidIdx == patternLen) return reinterpret_cast<uintptr_t>(region);

        const uint8_t needleByte = static_cast<uint8_t>(patternBytes[firstValidIdx]);
        const __m128i needleVec = _mm_set1_epi8(needleByte);

        const std::size_t maxScanIndex = regionSize - patternLen;

        //scan memory 16 bytes at a time
        std::size_t i = 0;
        for (; i + 16 <= maxScanIndex; i += 16) 
        {
            //load 16 bytes from mem
            __m128i memoryVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&region[i + firstValidIdx]));

            //compare all 16 bytes against the needle in 1 instruction
            __m128i cmpResult = _mm_cmpeq_epi8(needleVec, memoryVec);

            int mask = _mm_movemask_epi8(cmpResult);
            while (mask != 0)
            {
                unsigned long bitIndex;
                _BitScanForward(&bitIndex, mask);

                std::size_t candidateOffset = i + bitIndex;

                if (candidateOffset <= maxScanIndex)
                {
                    bool found = true;
                    for (std::size_t j = 0; j < patternLen; j++)
                    {
                        if (patternBytes[j] != -1 && region[candidateOffset + j] != static_cast<uint8_t>(patternBytes[j]))
                        {
                            found = false;
                            break;
                        }
                    }
                    if (found) {
                        return reinterpret_cast<uintptr_t>(&region[candidateOffset]);
                    }
                }
                mask &= mask - 1;
            }
        }

        //fallback for remaining bytes
        for (; i <= maxScanIndex; i++)
        {
            bool found = true;
            for (std::size_t j = 0; j < patternLen; j++)
            {
                if (patternBytes[j] != -1 && region[i + j] != static_cast<uint8_t>(patternBytes[j]))
                {
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

        return ScanRegionSSE2(reinterpret_cast<std::uint8_t*>(hModule), sizeOfImage, patternBytes);
    }

    //Bounded variant: searches only [startAddress, endAddress) inside the module image
    //(clamped to the image base/size). Used by MainIntroPatch, where the attract
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
        return ScanRegionSSE2(reinterpret_cast<std::uint8_t*>(startAddress), endAddress - startAddress, patternBytes);
    }
	

	//Finds a literal C-string (including its null terminator) anywhere in the
	//image. Unlike FindPattern, the needle is matched byte-for-byte with no
	//wildcards, so it can locate game data (e.g. checkpoint names) regardless of
	//how the code that references it is laid out in a given build.	
	
	uintptr_t FindString(HMODULE hModule, const char* needle)
	{
		auto dosHeader = (PIMAGE_DOS_HEADER)hModule;
		auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)hModule + dosHeader->e_lfanew);
		auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;

		auto bytes = std::vector<int>{};
		for (const char* c = needle; *c != '\0'; ++c)
			bytes.push_back(static_cast<unsigned char>(*c));
		bytes.push_back(0); //match the null terminator too, so we land on a real string start

		return ScanRegionSSE2(reinterpret_cast<std::uint8_t*>(hModule), sizeOfImage, bytes);
	}

    bool WriteBytes(uintptr_t address, const void* data, std::size_t size)
    {
        DWORD oldProtect;
        if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;
        memcpy(reinterpret_cast<void*>(address), data, size);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
        VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &oldProtect);
        return true;
    }
}
