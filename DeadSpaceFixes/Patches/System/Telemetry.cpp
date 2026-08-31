#include "common.h"
#include "Utils.h"

namespace Patches {

	namespace System {

		namespace Telemetry {

			//Disables the game's network/telemetry attempts so it stays fully offline
			//and starts a bit faster.

			WSAStartup_t oWSAStartup = nullptr;
			Netbios_t oNetbios = nullptr;

			//returns WSASYSNOTREADY (10091) so the game thinks the network is unavailable
			int WINAPI hkWSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData)
			{
				return 10091;
			}

			//returns NRC_SYSTEM (0x40) so it can't scan network devices
			UCHAR WINAPI hkNetbios(PNCB pncb)
			{
				return 0x40;
			}

			void Apply()
			{
				HMODULE hWinSock = GetModuleHandleA("ws2_32.dll");
				//GetModuleHandleA can fail, guard against the NULL deref before passing to GetProcAddress
				if (hWinSock != nullptr)
				{
					FARPROC pWSAStartup = GetProcAddress(hWinSock, "WSAStartup");
					if (pWSAStartup)
					{
						MH_CreateHook(pWSAStartup, &hkWSAStartup, reinterpret_cast<LPVOID*>(&oWSAStartup));
						MH_EnableHook(pWSAStartup);
						DEBUG_LOG("WSAStartup hooked, all network/telemetry blocked");
					}
				}
				//some games don't properly load this so we LoadLibrary it
				HMODULE hNetApi = LoadLibraryA("netapi32.dll");
				if (hNetApi != nullptr) //LoadLibraryA can also fail, same guard
				{
					FARPROC pNetbios = GetProcAddress(hNetApi, "Netbios");
					if (pNetbios)
					{
						MH_CreateHook(pNetbios, &hkNetbios, reinterpret_cast<LPVOID*>(&oNetbios));
						MH_EnableHook(pNetbios);
						DEBUG_LOG("Netbios also hooked, weird NAT harvester is now blocked");
					}
				}
			}
		}
	}
}
