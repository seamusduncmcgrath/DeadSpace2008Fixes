#include "common.h"
#include "Utils.h"
#include <mutex>

namespace {

	//Forwarding targets into the real xinput1_3.dll
	XInputGetState_t oXInputGetState = nullptr;
	XInputSetState_t oXInputSetState = nullptr;
	XInputGetCapabilities_t oXInputGetCapabilities = nullptr;

	SDL_Gamepad* g_CurrentGamepad = nullptr;
	std::mutex g_GamepadMutex;
	HANDLE g_ThreadHandle = nullptr;
	std::atomic<bool> g_Running{ true };

	DWORD WINAPI SDLDeviceThread(LPVOID)
	{
		SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			LOG_ERROR("[Features/Input/SdlGamepad]", "SDL failed to init!");
			return 1;
		}

		SDL_Event event;

		while (g_Running)
		{
			while (SDL_WaitEventTimeout(&event, 4))
			{
				if (event.type == SDL_EVENT_QUIT)
					break;

				else if (event.type == SDL_EVENT_GAMEPAD_ADDED) //controller plugged in
				{
					std::lock_guard<std::mutex> lock(g_GamepadMutex);
					if (g_CurrentGamepad == nullptr)
					{
						g_CurrentGamepad = SDL_OpenGamepad(event.gdevice.which); //we open the controller plugged in here
						if (g_CurrentGamepad)
						{
							const char* name = SDL_GetGamepadName(g_CurrentGamepad);
							LOG_INFO("[Features/Input/SdlGamepad]", "Controller connected: %s", name ? name : "Unknown");
							SDL_SetGamepadLED(g_CurrentGamepad, 0, 255, 255); //idk i just like the leds kinda like issacs health bar
						}
					}
				}
				else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) //controller removed
				{
					std::lock_guard<std::mutex> lock(g_GamepadMutex);
					SDL_Gamepad* disconnectedPad = SDL_GetGamepadFromID(event.gdevice.which);
					if (disconnectedPad == g_CurrentGamepad)
					{
						LOG_INFO("[Features/Input/SdlGamepad]", "Controller disconnected.");
						//clean up memory
						SDL_CloseGamepad(g_CurrentGamepad);
						g_CurrentGamepad = nullptr;
					}
				}
			}
		}

		{
			std::lock_guard<std::mutex> lock(g_GamepadMutex);
			if (g_CurrentGamepad != nullptr)
			{
				SDL_CloseGamepad(g_CurrentGamepad);
				g_CurrentGamepad = nullptr;
			}
		}

		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
		return 0;
	}

	bool HasSdlGamepadLocked()
	{
		return g_CurrentGamepad != nullptr && SDL_GamepadConnected(g_CurrentGamepad);
	}

}

namespace Features {

	namespace Input {

		namespace SdlGamepad {

			void StartThread()
			{
				//Load the real XInput DLL so unhandled calls can be forwarded to it.
				char syspath[MAX_PATH];
				GetSystemDirectoryA(syspath, MAX_PATH);
				strcat_s(syspath, "\\xinput1_3.dll");

				HMODULE hRealXInput = LoadLibraryA(syspath);
				if (hRealXInput)
				{
					oXInputGetState = (XInputGetState_t)GetProcAddress(hRealXInput, "XInputGetState");
					oXInputSetState = (XInputSetState_t)GetProcAddress(hRealXInput, "XInputSetState");
					oXInputGetCapabilities = (XInputGetCapabilities_t)GetProcAddress(hRealXInput, "XInputGetCapabilities");
				}

				g_ThreadHandle = CreateThread(nullptr, 0, SDLDeviceThread, nullptr, 0, nullptr);
			}

			void Shutdown()
			{
				SDL_Event quitEvent{};
				quitEvent.type = SDL_EVENT_QUIT;
				SDL_PushEvent(&quitEvent);

				if (g_ThreadHandle)
				{
					WaitForSingleObject(g_ThreadHandle, 2000);
					CloseHandle(g_ThreadHandle);
					g_ThreadHandle = nullptr;
				}
			}
		}
	}
}

extern "C" DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	if (dwUserIndex == 0)
	{
		std::lock_guard<std::mutex> lock(g_GamepadMutex);
		if (HasSdlGamepadLocked())
		{
			SDL_UpdateGamepads();

			XINPUT_GAMEPAD newGamepad{};

			//map the buttons
			// SDL's south/east/west/north convention maps to cross/circle/square/triangle.
			static constexpr struct { SDL_GamepadButton sdl; WORD xinput; } kButtonMap[] = {
				{ SDL_GAMEPAD_BUTTON_SOUTH,         XINPUT_GAMEPAD_A },
				{ SDL_GAMEPAD_BUTTON_EAST,          XINPUT_GAMEPAD_B },
				{ SDL_GAMEPAD_BUTTON_WEST,          XINPUT_GAMEPAD_X },
				{ SDL_GAMEPAD_BUTTON_NORTH,         XINPUT_GAMEPAD_Y },
				{ SDL_GAMEPAD_BUTTON_DPAD_UP,       XINPUT_GAMEPAD_DPAD_UP },
				{ SDL_GAMEPAD_BUTTON_DPAD_DOWN,     XINPUT_GAMEPAD_DPAD_DOWN },
				{ SDL_GAMEPAD_BUTTON_DPAD_LEFT,     XINPUT_GAMEPAD_DPAD_LEFT },
				{ SDL_GAMEPAD_BUTTON_DPAD_RIGHT,    XINPUT_GAMEPAD_DPAD_RIGHT },
				{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  XINPUT_GAMEPAD_LEFT_SHOULDER },  //L1
				{ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER },  //R1
				{ SDL_GAMEPAD_BUTTON_BACK,           XINPUT_GAMEPAD_BACK },
				{ SDL_GAMEPAD_BUTTON_START,          XINPUT_GAMEPAD_START },  //options/plus
				{ SDL_GAMEPAD_BUTTON_LEFT_STICK,    XINPUT_GAMEPAD_LEFT_THUMB },  //L3
				{ SDL_GAMEPAD_BUTTON_RIGHT_STICK,   XINPUT_GAMEPAD_RIGHT_THUMB },  //R3
			};

			WORD buttons = 0;
			for (const auto& b : kButtonMap)
				if (SDL_GetGamepadButton(g_CurrentGamepad, b.sdl))
					buttons |= b.xinput;

			newGamepad.wButtons = buttons;

			//map the triggers
			int16_t leftTriggerSDL = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
			int16_t rightTriggerSDL = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

			newGamepad.bLeftTrigger = static_cast<BYTE>((leftTriggerSDL * 255) / 32767);
			newGamepad.bRightTrigger = static_cast<BYTE>((rightTriggerSDL * 255) / 32767);

			//map the joysticks
			newGamepad.sThumbLX = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFTX);
			newGamepad.sThumbRX = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHTX);

			int16_t sdlLeftY = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFTY); //we need to fetch the raw y values or there cooked
			int16_t sdlRightY = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHTY);

			newGamepad.sThumbLY = (sdlLeftY == -32768) ? 32767 : -sdlLeftY; //then  we safely invert the y values here
			newGamepad.sThumbRY = (sdlRightY == -32768) ? 32767 : -sdlRightY;

			static DWORD s_PacketNumber = 1;
			static XINPUT_GAMEPAD s_LastGamepad{};
			if (memcmp(&newGamepad, &s_LastGamepad, sizeof(XINPUT_GAMEPAD)) != 0)
			{
				s_LastGamepad = newGamepad;
				++s_PacketNumber;
			}

			pState->dwPacketNumber = s_PacketNumber;
			pState->Gamepad = newGamepad;

			return ERROR_SUCCESS;
		}
	}
	if (oXInputGetState) return oXInputGetState(dwUserIndex, pState);
	return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
	if (dwUserIndex == 0)
	{
		std::lock_guard<std::mutex> lock(g_GamepadMutex);
		if (HasSdlGamepadLocked())
		{
			Uint16 lowFreq = pVibration->wLeftMotorSpeed; //heavy rumble
			Uint16 highFreq = pVibration->wRightMotorSpeed; //light rumble

			SDL_RumbleGamepad(g_CurrentGamepad, lowFreq, highFreq, 5000); //send the rumble to the controller for 5 seconds, the game should stop it early

			return ERROR_SUCCESS;
		}
	}
	if (oXInputSetState) return oXInputSetState(dwUserIndex, pVibration);
	return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities) 
{
	if (dwUserIndex == 0)
	{
		std::lock_guard<std::mutex> lock(g_GamepadMutex);
		if (HasSdlGamepadLocked())
		{
			memset(pCapabilities, 0, sizeof(XINPUT_CAPABILITIES));

			pCapabilities->Type = XINPUT_DEVTYPE_GAMEPAD;
			pCapabilities->SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
			pCapabilities->Flags = 0;

			pCapabilities->Gamepad.wButtons = 0xFFFF;
			pCapabilities->Gamepad.bLeftTrigger = 255;
			pCapabilities->Gamepad.bRightTrigger = 255;
			pCapabilities->Gamepad.sThumbLX = 32767;
			pCapabilities->Gamepad.sThumbLY = 32767;
			pCapabilities->Gamepad.sThumbRX = 32767;
			pCapabilities->Gamepad.sThumbRY = 32767;

			pCapabilities->Vibration.wLeftMotorSpeed = 65535;
			pCapabilities->Vibration.wRightMotorSpeed = 65535;

			return ERROR_SUCCESS;
		}
	}
	if (oXInputGetCapabilities) return oXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);
	return ERROR_DEVICE_NOT_CONNECTED;
}
