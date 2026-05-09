# DeadSpace2008Fixes

A mod for Dead Space (2008) that adds numerous fixes and improvements

## Features

* **Borderless Windowed:** Adds a simple borderless windowed mode that fixes gamma issues and alt-tabbing bugs. To use, simply disable fullscreen in the game settings
* **(Experimental) Fixed Crashes On 10+ Core CPUs:** The crashes on modern 10+ core CPUs should be fixed. (I do not own a 10+ core CPU to 100% confirm, but the array overflow has been patched).
* **Reduced Issues At High FPS:** A large amount of the physics/ragdoll issues at 60+ FPS are caused by the game relying on GetTickCount, which is really inaccurate. I found an unused developer flag in the engine that allows you to swap GetTickCount for QueryPerformanceCounter, which is much more precise. Visceral likely disabled this because older AMD CPUs had desync issues, but on modern hardware, it works perfectly. *Note:* I still recommend capping the FPS to around 120-180. I was able to complete QTEs and had no issues with ragdolls at a 180 FPS cap, but if it goes above ~200 FPS, the issues start to come back.
* **Significantly Faster Startup Times:** Removed the insanely slow check for legacy DirectInput8 devices, this takes about 5 seconds off the boot time! This does mean the game won't pick up steering wheels or pre-2007 joysticks, but any modern Xbox/PlayStation controllers running through XInput will still work perfectly.
* **Anisitropic Filtering:** Hooks the D3D texture sampler to force 16x anisotropic filtering. This has zero performance loss on modern hardware and hugely improves texture clarity.
* **High Resolution Subtitle Fix:** Fixes the too small subtitles at resolutions above 720p, no need to squint to see subtitles at 4k anymore!
* **Safer Save String Handling:** Improved the save string handling; Viceral had left an issue where it would clear 128 bytes rather than 128 wide characters (256 bytes), which would cause garbage data and could cause issues and crashes.
* **Removed Telemetry:** Removed some random telemetry that would tell EA what OS you are on and get your device MAC address. This will make the game fully offline and reduce startup times a bit.
* **Native PS4/5 & Switch Controller Support:** The game now supports PS4/5 and Nintendo switch controllers using SDL3.


## Installation

1. Download the latest release of this mod from the Releases tab.
2. Extract the downloaded file.
3. Place `xinput1_3.dll` and `SDL3.dll` directly into your Dead Space game folder (where `Dead Space.exe` is located).
4. Launch the game, and on the bottom right of the main menu should say "DeadSpaceFixes Installed!" rather than the game version number.


## Comparison Of Texture Filtering On Vs Off
**On:**
<img width="1919" height="1079" alt="on" src="https://github.com/user-attachments/assets/c93687c0-b7e0-421c-8e58-8c5f66e1f504" />
**Off:**
<img width="1913" height="1052" alt="off" src="https://github.com/user-attachments/assets/45e88b85-bcc5-42fb-b4cc-9212a8a837fe" />

## Comparison of subtitles at 4k
**Fix On:**
<img width="3819" height="2064" alt="image" src="https://github.com/user-attachments/assets/e4ec2daf-c264-44d1-b503-018d64f2fa5e" />
**Fix Off:**
<img width="3839" height="2159" alt="image" src="https://github.com/user-attachments/assets/d8ec2329-fc4a-47fb-b65c-d4b6dc9291bc" />
