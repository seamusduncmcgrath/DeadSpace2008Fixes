# DeadSpace2008Fixes

A ASI plugin for Dead Space (2008) that adds proper borderless windowed, as well as 16x anisotropic filtering and numerous other fixes and improvements

## Features.

* **Borderless Windowed:** Adds a simple borderless windowed mode that fixes gamma issues and alt-tabbing bugs. To use, simply disable fullscreen in the game settings
* **(Experimental) Fixed Crashes On 10+ Core CPU's:** The crashes on modern 10+ core CPUs should be completely fixed. (I do not own a 10+ core CPU to 100% confirm, but the array overflow has been patched).
* **Reduced Issues At High FPS:** A large amount of the physics/ragdoll issues at 60+ FPS are caused by the game relying on GetTickCount, which is really inaccurate. I found an unused developer flag in the engine that allows you to swap GetTickCount for QueryPerformanceCounter, which is much more precise. Visceral likely disabled this because older AMD CPUs had desync issues, but on modern hardware, it works perfectly. *Note:* I still recommend capping the FPS to around 120-180. I was able to complete QTEs and had no issues with ragdolls at a 180 FPS cap, but if it goes above ~200 FPS, the issues start to come back.
* **Significantly Faster Startup Times:** Removed the insanely slow check for legacy DirectInput8 devices, this takes about 5 seconds off the boot time! This does mean the game won't pick up steering wheels or pre-2007 joysticks, but reall any modern Xbox/PlayStation controllers running through XInput will still work perfectly.
* **Anisitropic Filtering:** Hooks the D3D texture sampler to force 16x anisotropic filtering. This has zero performance loss on modern hardware and hugely improves texture clarity.
* **High Resolution Subtitle Fix:** Fixes the too small subtitles at resolutions above 720p, no need to squint to see subtitles at 4k anymore!


## Installation

This mod requires an ASI loader to inject into the game.

1. Download the **32-bit (x86)** version of [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
2. Extract the downloaded file and rename it to `dinput8.dll`.
3. Download the latest release of this mod from the Releases tab.
4. Place `dinput8.dll` and the downloaded `.asi` plugin directly into your Dead Space game folder (where `Dead Space.exe` is located).
5. Launch the game.


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
