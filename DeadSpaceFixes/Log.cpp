#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <cstdio>
#include <cstdarg>

#include "Log.h"

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
}