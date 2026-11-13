#include "common.h"
#include "Utils.h"
#include "Config.h"

#include <algorithm>

namespace Fixes {

	namespace Physics {

		namespace Timer {

			static constexpr const char* kTag = "[Fixes/Physics/Timer]";

			const int kAverageFrames = 5;          //same as the game
			const double kMinimumFrameMs = 1.0;    //the shortest frame the game ever sees

			typedef void(__fastcall* FrameTimer_t)(void* pTimer, void* edx, int unknown);
			FrameTimer_t oFrameTimer = nullptr;

			BYTE* g_pFixedStep = nullptr;      //set while booting, the timer counts whole refresh periods then
			DWORD* g_pMaxFrameMs = nullptr;    //longest frame the game will count
			float* g_pPeriodMs = nullptr;      //one display refresh in ms
			float* g_pTimeScale = nullptr;
			DWORD* g_pTimerUpdates = nullptr;  //goes up whenever the timer runs while the game is running

			//real time, not scaled
			float* g_pFrameDt = nullptr;       //seconds
			DWORD* g_pRealMs = nullptr;
			float* g_pRealSeconds = nullptr;
			DWORD* g_pRealPeriods = nullptr;   //in display refreshes

			//game time, scaled by the time scale
			float* g_pScaledDt = nullptr;      //seconds
			uint64_t* g_pGameUs = nullptr;
			DWORD* g_pGameMs = nullptr;
			float* g_pGameSeconds = nullptr;
			DWORD* g_pGameDtMs = nullptr;
			DWORD* g_pGameFrames = nullptr;    //only goes up when the game clock moved

			LARGE_INTEGER g_Frequency = {};
			LARGE_INTEGER g_LastCounter = {};
			bool g_HaveLastCounter = false;
			bool g_WasCounting = false;            //the last call counted a frame, so the next one waits out 1 ms

			double g_RecentFrames[kAverageFrames] = {};
			int g_RecentFrameCount = 0;
			int g_NextRecentFrame = 0;

			double g_RealMsRemainder = 0.0;
			double g_GameUsRemainder = 0.0;

			void ForgetRecentFrames()
			{
				g_RecentFrameCount = 0;
				g_NextRecentFrame = 0;
			}

			double AverageFrameMs(double frameMs)
			{
				g_RecentFrames[g_NextRecentFrame] = frameMs;
				g_NextRecentFrame = (g_NextRecentFrame + 1) % kAverageFrames;
				if (g_RecentFrameCount < kAverageFrames)
					++g_RecentFrameCount;

				double total = 0.0;
				for (int i = 0; i < g_RecentFrameCount; ++i)
					total += g_RecentFrames[i];
				return total / g_RecentFrameCount;
			}

			void __fastcall hkFrameTimer(void* pTimer, void* edx, int unknown)
			{
				LARGE_INTEGER counter;
				QueryPerformanceCounter(&counter);

				//running faster than 1000 FPS, wait until a millisecond has passed since the last frame
				if (g_WasCounting && g_HaveLastCounter)
				{
					LONGLONG shortestFrame = static_cast<LONGLONG>(g_Frequency.QuadPart * kMinimumFrameMs / 1000.0);
					while (counter.QuadPart - g_LastCounter.QuadPart < shortestFrame)
					{
						YieldProcessor();
						QueryPerformanceCounter(&counter);
					}
				}

				double sinceLastMs = g_HaveLastCounter
					? static_cast<double>(counter.QuadPart - g_LastCounter.QuadPart) * 1000.0 / static_cast<double>(g_Frequency.QuadPart)
					: -1.0;
				g_LastCounter = counter;
				g_HaveLastCounter = true;

				DWORD timerUpdatesBefore = *g_pTimerUpdates;
				DWORD gameFramesBefore = *g_pGameFrames;
				DWORD realMsBefore = *g_pRealMs;
				uint64_t gameUsBefore = *g_pGameUs;
				DWORD gameMsBefore = *g_pGameMs;

				oFrameTimer(pTimer, edx, unknown);

				//booting, or the game isn't running (loading, paused) so the timer didn't count anything
				g_WasCounting = !*g_pFixedStep && *g_pTimerUpdates != timerUpdatesBefore;
				if (!g_WasCounting)
				{
					ForgetRecentFrames();
					return;
				}

				//the first frame we see, use what the game worked out
				double frameMs = sinceLastMs >= 0.0 ? sinceLastMs : *g_pFrameDt * 1000.0;
				frameMs = (std::min)(frameMs, static_cast<double>(*g_pMaxFrameMs));
				frameMs = (std::max)(frameMs, kMinimumFrameMs);
				frameMs = AverageFrameMs(frameMs);

				//real time, carrying the part of a millisecond the game's counters can't hold to the next frame
				g_RealMsRemainder += frameMs;
				DWORD wholeMs = static_cast<DWORD>(g_RealMsRemainder);
				g_RealMsRemainder -= wholeMs;
				DWORD realMs = realMsBefore + wholeMs;

				*g_pFrameDt = static_cast<float>(frameMs * 0.001);
				*g_pRealMs = realMs;
				*g_pRealSeconds = static_cast<float>((realMs + g_RealMsRemainder) * 0.001);
				if (*g_pPeriodMs > 0.0f)
					*g_pRealPeriods = static_cast<DWORD>(realMs / *g_pPeriodMs + 0.5);

				//the game holds its clock (g_pGameFrames doesn't move) at times, leave it held
				if (*g_pGameFrames == gameFramesBefore)
					return;

				double scaledMs = *g_pTimeScale * frameMs;
				g_GameUsRemainder += (std::max)(scaledMs, 0.0) * 1000.0;
				uint64_t wholeUs = static_cast<uint64_t>(g_GameUsRemainder);
				g_GameUsRemainder -= static_cast<double>(wholeUs);
				uint64_t gameUs = gameUsBefore + wholeUs;
				DWORD gameMs = static_cast<DWORD>((gameUs + 500) / 1000);

				*g_pScaledDt = static_cast<float>(scaledMs * 0.001);
				*g_pGameUs = gameUs;
				*g_pGameMs = gameMs;
				*g_pGameSeconds = static_cast<float>(static_cast<double>(gameUs) * 0.000001);
				*g_pGameDtMs = gameMs - gameMsBefore; //whole ms the clock moved, so these add up to the clock
			}

			//yucky
			template <typename T>
			bool ReadAddress(HMODULE hExe, uintptr_t function, const char* pSignature, int offset, T*& pOut)
			{
				uintptr_t match = Utils::FindPattern(hExe, pSignature, function, function + 0x500);
				if (match == 0)
				{
					LOG_WARN(kTag, "Couldn't find %s in the frame timer", pSignature);
					return false;
				}
				pOut = *reinterpret_cast<T**>(match + offset);
				return true;
			}

			void Apply(HMODULE hExe)
			{
				if (!Config::Fixes::GameTimerFix)
					return;

				uintptr_t frameTimer = Utils::FindPattern(hExe, "55 8B EC 83 E4 F8 83 EC 20 53 55 56 57 6A 08 8D 44 24 24 8B F9 50 89 7C 24 34 FF 15");
				if (frameTimer == 0)
				{
					LOG_WARN(kTag, "Couldn't find the frame timer");
					return;
				}

				uintptr_t resetRef = Utils::FindPattern(hExe, "38 1D ? ? ? ? DF 7C 24 08 F3 0F 11 05 ? ? ? ? F3 0F 11 05 ? ? ? ?");
				if (resetRef == 0)
				{
					LOG_WARN(kTag, "Couldn't find the clock reset");
					return;
				}
				g_pFixedStep = *reinterpret_cast<BYTE**>(resetRef + 2);
				g_pScaledDt = *reinterpret_cast<float**>(resetRef + 22);

				bool found = true;
				//the variable step path: mov eax, [maxFrameMs] ... call push / call average / mov ecx, eax / mov eax, [realMs]
				found &= ReadAddress(hExe, frameTimer, "A1 ? ? ? ? DD D8 3B C8 0F 47 C8 E8 ? ? ? ? E8 ? ? ? ? 8B C8 A1 ? ? ? ? 03 C1 85 C0", 1, g_pMaxFrameMs);
				found &= ReadAddress(hExe, frameTimer, "A1 ? ? ? ? DD D8 3B C8 0F 47 C8 E8 ? ? ? ? E8 ? ? ? ? 8B C8 A1 ? ? ? ? 03 C1 85 C0", 25, g_pRealMs);
				//fld [periodMs] / fdiv st(1), st / fld 0.5 / fnstcw / fadd st(2), st
				found &= ReadAddress(hExe, frameTimer, "D9 05 ? ? ? ? DC F9 D9 05 ? ? ? ? D9 7C 24 14 DC C2", 2, g_pPeriodMs);
				//mov [realPeriods], edx / fldcw / fld 0.001 / fmul / fxch / fstp [realSeconds]
				found &= ReadAddress(hExe, frameTimer, "89 15 ? ? ? ? D9 6C 24 14 D9 05 ? ? ? ? DC CB D9 CB D9 1D ? ? ? ?", 2, g_pRealPeriods);
				found &= ReadAddress(hExe, frameTimer, "89 15 ? ? ? ? D9 6C 24 14 D9 05 ? ? ? ? DC CB D9 CB D9 1D ? ? ? ?", 22, g_pRealSeconds);
				//mov eax, 1 / add [timerUpdates], eax / cmp [clockHeld], eax / jz / movss xmm0, [timeScale] / mulss xmm0, [frameDt]
				const char* commonSig = "B8 01 00 00 00 01 05 ? ? ? ? 39 05 ? ? ? ? 0F 84 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 59 05 ? ? ? ?";
				found &= ReadAddress(hExe, frameTimer, commonSig, 7, g_pTimerUpdates);
				found &= ReadAddress(hExe, frameTimer, commonSig, 27, g_pTimeScale);
				found &= ReadAddress(hExe, frameTimer, commonSig, 35, g_pFrameDt);
				//mov ecx, [gameUs] / mov esi, [gameUs + 4] / mov edi, eax / add ecx, edi / adc esi, edx
				found &= ReadAddress(hExe, frameTimer, "8B 0D ? ? ? ? 8B 35 ? ? ? ? 8B F8 03 CF 13 F2", 2, g_pGameUs);
				//mov [gameMs], eax / mov eax, esi / and esi, 7FFFFFFFh
				found &= ReadAddress(hExe, frameTimer, "A3 ? ? ? ? 8B C6 81 E6 FF FF FF 7F", 1, g_pGameMs);
				//fmul 0.000001 / fstp [gameSeconds] / jz
				found &= ReadAddress(hExe, frameTimer, "DC 0D ? ? ? ? D9 1D ? ? ? ? 74 40", 8, g_pGameSeconds);
				//mov [gameDtMs], edx / fldcw / mov eax, [esp+28h] / mov ecx, [esp+2Ch] / add [gameFrames], 1
				found &= ReadAddress(hExe, frameTimer, "89 15 ? ? ? ? D9 6C 24 14 8B 44 24 28 8B 4C 24 2C 83 05 ? ? ? ? 01", 2, g_pGameDtMs);
				found &= ReadAddress(hExe, frameTimer, "89 15 ? ? ? ? D9 6C 24 14 8B 44 24 28 8B 4C 24 2C 83 05 ? ? ? ? 01", 20, g_pGameFrames);
				if (!found)
				{
					LOG_WARN(kTag, "Couldn't find everything the frame timer writes, not patching");
					return;
				}

				QueryPerformanceFrequency(&g_Frequency);

				void* pTarget = reinterpret_cast<void*>(frameTimer);
				if (MH_CreateHook(pTarget, &hkFrameTimer, reinterpret_cast<LPVOID*>(&oFrameTimer)) != MH_OK || MH_EnableHook(pTarget) != MH_OK)
				{
					LOG_WARN(kTag, "Couldn't hook the frame timer at 0x%X", frameTimer);
					return;
				}

				LOG_INFO(kTag, "Hooked the frame timer at 0x%X, frame times now come from QueryPerformanceCounter", frameTimer);
			}
		}
	}
}
