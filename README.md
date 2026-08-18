# DeadSpace2008Fixes

A mod for Dead Space (2008) that adds numerous fixes and improvements

# Features 

## Borderless Windowed
Adds a simple borderless windowed mode that fixes gamma issues and alt-tabbing bugs. To use, simply disable fullscreen in the game settings.
> [!WARNING]
 Set BorderlessWindowed to 0 in the `DeadSpaceFixes.ini` if you plan on not using borderless!
## Reduced Issues At High FPS
A large amount of the physics/ragdoll issues at 60+ FPS are caused by the game relying on GetTickCount, which is really inaccurate. I found an unused developer flag in the engine that allows you to swap GetTickCount for QueryPerformanceCounter, which is much more precise. Visceral likely disabled this because older AMD CPUs had desync issues, but on modern hardware, it works perfectly. *Note:* I still recommend capping the FPS to around 120-180. I was able to complete QTEs and had no issues with ragdolls at a 180 FPS cap, but if it goes above ~200 FPS, the issues start to come back.

## Anisotropic Filtering
Hooks the D3D texture sampler to force 16x anisotropic filtering. This has zero performance loss on modern hardware and hugely improves texture clarity.
<div align="center">
  <table>
    <tr>
      <td width="10%"><img style="width:100%" src="https://github.com/user-attachments/assets/c93687c0-b7e0-421c-8e58-8c5f66e1f504"></td>
      <td width="10%"><img style="width:100%" src="https://github.com/user-attachments/assets/45e88b85-bcc5-42fb-b4cc-9212a8a837fe"></td>
    </tr>
    <tr>
      <td align="center">On</td>
      <td align="center">Off</td>
    </tr>
  </table>
</div>

## Fixed Crashes on 10+ Core CPUs
The crashes on modern 10+ core CPUs should be fixed. (I do not own a 10+ core CPU to 100% confirm, but there's no reason for the patch not to work).

## Significantly Faster Startup Times
Removed the insanely slow check for legacy DirectInput8 devices; this saves about 3 seconds off the boot time! (depends on how many HID devices are connected) This does mean the game won't pick up steering wheels or pre-2007 joysticks, but any modern Xbox/PlayStation controllers running through XInput will still work perfectly.

## High Resolution Subtitle Fix
Fixes the too small subtitles at resolutions above 720p; no need to squint to see subtitles at 4k anymore!
<div align="center">
  <table>
    <tr>
        <td width="10%"><img style="width:100%" src="https://github.com/user-attachments/assets/d8ec2329-fc4a-47fb-b65c-d4b6dc9291bc"></td>
        <td width="10%"><img style="width:100%" src="https://github.com/user-attachments/assets/e4ec2daf-c264-44d1-b503-018d64f2fa5e"></td>
    </tr>
    <tr>
      <td align="center">Vanilla 4k</td>
      <td align="center">DeadSpaceFixes 4k</td>
    </tr>
  </table>
</div>

## Safer Save String Handling
Improved the save string handling; Viceral had left an issue where it would clear 128 bytes rather than 128 wide characters (256 bytes), which would cause garbage data and could cause issues and crashes.

## Removed Telemetry
Removed some random telemetry that would tell EA what OS you are on and get your device MAC address. This will make the game fully offline and reduce startup times a bit.

## Native PS4/5 & Switch Controller Support
The game now supports PS4/5 and Nintendo switch controllers using SDL3.

## Fixed VSync
The 30 FPS cap on the VSync has been removed, though the game uses half rate VSync so it will be capped to half your display refresh rate.

## (Optional) 60 FPS Cap
Simply change `SafeFPSCap=0` to `SafeFPSCap=1` in the `DeadSpaceFixes.ini`

## (Optional) Skip Ishimura Landing Cutscene
Skips the Ishimura landing cutscene to help with speedrunning the game.
  
## (Optional) Skip Intro To Main Menu
Skips the boot intro and lands directly on the main menu. Set `SkipIntroToMainMenu=1` in the config file.

## (Optional) Skip Loading Screen Delay
Shortens the artificial wait on the loading screen once the level has finished loading when starting a new game. The vanilla game holds the loading screen on a ~30 second tip-cycling loop even after the level is ready; this cuts that hold to ~3 seconds, so the loading tips still appear but the screen drops almost immediately. Set `SkipLoadingScreenDelay=1` in the config file.


# Install Guide

> [!NOTE]  
> Requires at least Windows 7 SP1 x86 and a CPU with SSE2 support.
>
> **Download** [DeadSpaceFixes](https://github.com/seamusduncmcgrath/DeadSpace2008Fixes/releases/latest/download/xinput1_3.dll)
> 
> Simply add the `xinput1_3.dll` `SDL3.dll` and files you downloaded from above into the game folder next to `Dead Space.exe`, and if it worked yoo should see "DeadSpaceFixes Installed!" in the bottom right of the main menu instead of the version number.


# Config
Some settings can be enabled or disabled using `DeadSpaceFixes.ini`. Create this file in the game directory and add the following: (unless it already exists)

```ini
[Settings]
;Changes the games windowed mode to borderless windowed
BorderlessWindowed=1
;Removes the 30 FPS cap when VSync is enabled.
FixVSync=1
;Removes legacy DirectInput8 controller support, this reduces startup times and stuttering
PatchOutDInput8=1
;Removes EA's telemetry
RemoveTelemetry=1
;Enables a 60 FPS cap
SafeFPSCap=1
;Fixes subtitles being to small at high resolutions
FixSubtitleScale=1
;Skips the Ishimura landing cutscene to help with speedrunning
SkipIshimuraLandingCutscene=0
;Skips the boot intro and lands directly on the main menu
SkipIntroToMainMenu=0
;Skips the artificial wait on the loading screen once the level is ready on new game
SkipLoadingScreenDelay=0
```

## Credits
- [SDL3](https://github.com/libsdl-org/SDL) for the PS4/PS5 and switch controller support.
- [MinHook](https://github.com/tsudakageyu/minhook) for hooking.
- [SuiMachine](https://github.com/SuiMachine/Dead-Space---Intro-Skip) for the original intro skip mod.
- [MarkerPatch](https://github.com/Wemino/MarkerPatch) for the save string handling fix and reverse engineering references.

