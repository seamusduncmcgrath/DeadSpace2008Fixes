#include "common.h"
#include "Utils.h"

//Native PS4/5 & Switch controller support via SDL3.
//
//This DLL is a proxy for the real xinput1_3.dll (see proxy.def), so the
//XInputGetState/SetState/GetCapabilities functions below are exported and called
//by the game. When a gamepad managed by SDL is connected we serve the input from
//SDL; otherwise we forward to the real XInput DLL loaded from the system folder.

namespace {

	//Forwarding targets into the real xinput1_3.dll
	XInputGetState_t oXInputGetState = nullptr;
	XInputSetState_t oXInputSetState = nullptr;
	XInputGetCapabilities_t oXInputGetCapabilities = nullptr;

	SDL_Gamepad* g_CurrentGamepad = nullptr;

	DWORD WINAPI SDLDeviceThread(LPVOID)
	{
		if (!SDL_Init(SDL_INIT_GAMEPAD))
		{
			DEBUG_LOG("SDL failed to init!");
			return 1;
		}

		SDL_Event event;

		//SDL_WaitEventTimeout pumps the event queue without burning your CPU, it checks for events every 10 milliseconds
		while (SDL_WaitEventTimeout(&event, 10))
		{
			if (event.type == SDL_EVENT_GAMEPAD_ADDED) //controller plugged in
			{
				if (g_CurrentGamepad == nullptr)
				{
					g_CurrentGamepad = SDL_OpenGamepad(event.gdevice.which); //we open the controller plugged in here
					const char* name = SDL_GetGamepadName(g_CurrentGamepad);
					DEBUG_LOG("Controller connected: %s", name ? name : "Unknown");
					SDL_SetGamepadLED(g_CurrentGamepad, 0, 255, 255); //idk i just like the leds kinda like issacs health bar
				}
			}
			else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) //controller removed
			{
				SDL_Gamepad* disconnectedPad = SDL_GetGamepadFromID(event.gdevice.which);
				if (disconnectedPad == g_CurrentGamepad)
				{
					//clean up memory
					SDL_CloseGamepad(g_CurrentGamepad);
					g_CurrentGamepad = nullptr;
				}
			}
		}
		return 0;
	}

	bool HasSdlGamepad()
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

				CreateThread(nullptr, 0, SDLDeviceThread, nullptr, 0, nullptr);
			}

			void Shutdown()
			{
				if (g_CurrentGamepad != nullptr)
				{
					SDL_CloseGamepad(g_CurrentGamepad);
					g_CurrentGamepad = nullptr;
				}
			}
		}
	}
}

extern "C" DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	if (dwUserIndex == 0 && HasSdlGamepad())
	{
		memset(pState, 0, sizeof(XINPUT_STATE)); //clear the games struct so we start fresh

		static DWORD s_PacketNumber = 0;
		pState->dwPacketNumber = s_PacketNumber++;

		//map the buttons
		WORD buttons = 0;
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_SOUTH)) buttons |= XINPUT_GAMEPAD_A; //cross/b
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_EAST))  buttons |= XINPUT_GAMEPAD_B; //circle/a
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_WEST))  buttons |= XINPUT_GAMEPAD_X; //square/y
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_NORTH)) buttons |= XINPUT_GAMEPAD_Y; //triangle/x

		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_UP))    buttons |= XINPUT_GAMEPAD_DPAD_UP;
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN))  buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT))  buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;

		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))  buttons |= XINPUT_GAMEPAD_LEFT_SHOULDER; //L1
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)) buttons |= XINPUT_GAMEPAD_RIGHT_SHOULDER; //R1
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_START)) buttons |= XINPUT_GAMEPAD_START; //options/plus

		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK))  buttons |= XINPUT_GAMEPAD_LEFT_THUMB;  // L3 / left stick click, nearlly forgot about these
		if (SDL_GetGamepadButton(g_CurrentGamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)) buttons |= XINPUT_GAMEPAD_RIGHT_THUMB; // R3 / right stick click

		pState->Gamepad.wButtons = buttons;

		//map the triggers
		int16_t leftTriggerSDL = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
		int16_t rightTriggerSDL = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

		pState->Gamepad.bLeftTrigger = (BYTE)((leftTriggerSDL / 32767.0f) * 255.0f);
		pState->Gamepad.bRightTrigger = (BYTE)((rightTriggerSDL / 32767.0f) * 255.0f);

		//map the joysticks
		pState->Gamepad.sThumbLX = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFTX);
		pState->Gamepad.sThumbRX = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHTX);

		int16_t sdlLeftY = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_LEFTY); //we need to fetch the raw y values or there cooked
		int16_t sdlRightY = SDL_GetGamepadAxis(g_CurrentGamepad, SDL_GAMEPAD_AXIS_RIGHTY);

		pState->Gamepad.sThumbLY = (sdlLeftY == -32768) ? 32767 : -sdlLeftY; //then  we safely invert the y values here
		pState->Gamepad.sThumbRY = (sdlRightY == -32768) ? 32767 : -sdlRightY;

		return ERROR_SUCCESS;
	}
	if (oXInputGetState) return oXInputGetState(dwUserIndex, pState);
	return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
	if (dwUserIndex == 0 && HasSdlGamepad())
	{
		Uint16 lowFreq = pVibration->wLeftMotorSpeed; //heavy rumble
		Uint16 highFreq = pVibration->wRightMotorSpeed; //light rumble

		SDL_RumbleGamepad(g_CurrentGamepad, lowFreq, highFreq, 5000); //send the rumble to the controller for 5 seconds, the game should stop it early

		return ERROR_SUCCESS;
	}
	if (oXInputSetState) return oXInputSetState(dwUserIndex, pVibration);
	return ERROR_DEVICE_NOT_CONNECTED;
}

extern "C" DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities) //maybe rewrite soon? Don't think this is great
{
	if (dwUserIndex == 0 && HasSdlGamepad())
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
	if (oXInputGetCapabilities) return oXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);
	return ERROR_DEVICE_NOT_CONNECTED;
}
