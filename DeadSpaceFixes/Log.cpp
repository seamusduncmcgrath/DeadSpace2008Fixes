#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "Log.h"

namespace Utils {

	namespace {
		// Only messages at or above this level are written. Debug builds get everything,
		// release builds skip the verbose "Found <addr>" DEBUG lines.
#ifdef _DEBUG
		LogLevel g_LogLevel = LogLevel::Debug;
#else
		LogLevel g_LogLevel = LogLevel::Info;
#endif

		const char* const kLogName = "DeadSpaceFixes.log";

		//modules log from the game's thread, the SDL thread and the D3D9 thread
		SRWLOCK g_LogLock = SRWLOCK_INIT;
		FILE* g_pLogFile = nullptr;
		bool g_TriedOpening = false;
		bool g_HaveConsole = false;
		ULONGLONG g_StartTick = 0;

		//opens the log next to the game's exe the first time anything is logged
		void OpenLogFile()
		{
			g_TriedOpening = true;
			g_StartTick = GetTickCount64();

			char path[MAX_PATH] = {};
			DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
			char* pSlash = strrchr(path, '\\');
			if (length == 0 || length >= MAX_PATH || !pSlash)
				return;
			strcpy_s(pSlash + 1, MAX_PATH - (pSlash + 1 - path), kLogName);

			fopen_s(&g_pLogFile, path, "w");
		}
	}

	void InitialiseConsole()
	{
		AllocConsole();
		FILE* fDummy;
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);

		AcquireSRWLockExclusive(&g_LogLock);
		g_HaveConsole = true;
		ReleaseSRWLockExclusive(&g_LogLock);
	}

	inline void SetLogLevel(LogLevel level)
	{
		g_LogLevel = level;
	}

	void Log(LogLevel level, const char* tag, const char* fmt, ...)
	{
		if (level < g_LogLevel)
			return;

		static const char* kLevelNames[] = { "DBG", "INF", "WRN", "ERR" };
		const char* levelName = (level >= LogLevel::Debug && level <= LogLevel::Error)
			? kLevelNames[static_cast<int>(level)] : "???";

		char message[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(message, sizeof(message), fmt, args);
		va_end(args);

		AcquireSRWLockExclusive(&g_LogLock);

		if (!g_TriedOpening)
			OpenLogFile();

		if (g_pLogFile)
		{
			ULONGLONG ms = GetTickCount64() - g_StartTick;
			fprintf(g_pLogFile, "[%6llu.%03llu] [%s] %s %s\n", ms / 1000, ms % 1000, levelName, tag ? tag : "", message);
			fflush(g_pLogFile);
		}

		if (g_HaveConsole)
			printf("[DeadSpaceFixes] [%s] %s %s\n", levelName, tag ? tag : "", message);

		ReleaseSRWLockExclusive(&g_LogLock);
	}

	void LogStartupInfo(HMODULE hModule)
	{
		static constexpr const char* kTag = "[Startup]";

#ifdef _DEBUG
		LOG_INFO(kTag, "DeadSpaceFixes debug build, built " __DATE__ " " __TIME__);
#else
		LOG_INFO(kTag, "DeadSpaceFixes release build, built " __DATE__ " " __TIME__);
#endif

		char path[MAX_PATH] = {};
		GetModuleFileNameA(hModule, path, MAX_PATH);
		LOG_INFO(kTag, "Mod: %s", path);

		GetModuleFileNameA(nullptr, path, MAX_PATH);
		LOG_INFO(kTag, "Game: %s", path);

		//check the build, EA and GOG is the same excluding DRM, and steam is a later build
		HMODULE hExe = GetModuleHandleA(nullptr);

		auto pDos = reinterpret_cast<IMAGE_DOS_HEADER*>(hExe);
		auto pNt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<BYTE*>(hExe) + pDos->e_lfanew);

		DWORD timestamp = pNt->FileHeader.TimeDateStamp;
		DWORD imageSize = pNt->OptionalHeader.SizeOfImage;

		//the DRM wrappers add a section of their own: .bind is Steam's SteamStub, .ooa is the EA app's
		char sections[128] = {};
		bool steamStub = false;
		auto pSection = IMAGE_FIRST_SECTION(pNt);
		for (WORD i = 0; i < pNt->FileHeader.NumberOfSections; i++, pSection++)
		{
			char name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
			memcpy(name, pSection->Name, IMAGE_SIZEOF_SHORT_NAME);
			if (strcmp(name, ".bind") == 0)
				steamStub = true;
			if (i > 0)
				strcat_s(sections, " ");
			strcat_s(sections, name);
		}

		const char* build = "unknown";
		if (timestamp == 0x48DBCF01)
			build = imageSize == 0xB72000 ? "GOG (Sep 2008)" : "EA app (Sep 2008, same code as GOG)";

		else if (timestamp == 0x493E043A)
			build = steamStub ? "Steam (Dec 2008) with SteamStub DRM" : "Steam (Dec 2008) without DRM";

		LOG_INFO(kTag, "Game build: %s, timestamp 0x%08X, image size 0x%X, sections %s", build, timestamp, imageSize, sections);

		typedef LONG(WINAPI* RtlGetVersion_t)(OSVERSIONINFOW*);
		OSVERSIONINFOW version = {};
		version.dwOSVersionInfoSize = sizeof(version);
		auto pRtlGetVersion = reinterpret_cast<RtlGetVersion_t>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion"));
		if (pRtlGetVersion && pRtlGetVersion(&version) == 0)
			LOG_INFO(kTag, "Windows %lu.%lu build %lu", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);

		if (GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version"))
			LOG_INFO(kTag, "Running under Wine/Proton");
	}
}
