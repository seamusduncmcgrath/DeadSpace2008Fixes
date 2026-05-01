# DeadSpace2008Fixes

A simple ASI plugin for Dead Space (2008) that adds proper borderless windowed, as well as 16x anisitropic filtering 

## Features.

* **Borderless Windowed:** Add a simple borderless windowed mode that fixes the gamma issues and alt-tabbing. To use simply disable fullscreen in the game settings.
* **Anisitropic Filtering:** Hooks the D3D texture sampler to force 16x anisitropic filtering, this will have no performance loss on modern hardware and hugely improve texture clarity.

If I get around to it there should be more features soon, probably will be a mouse fix and intro video skip

## Comparison Of Texture Filtering On Vs Off
**On:**
<img width="1919" height="1079" alt="on" src="https://github.com/user-attachments/assets/c93687c0-b7e0-421c-8e58-8c5f66e1f504" />
**Off:**
<img width="1913" height="1052" alt="off" src="https://github.com/user-attachments/assets/45e88b85-bcc5-42fb-b4cc-9212a8a837fe" />


## Installation

This mod requires an ASI loader to inject into the game.

1. Download the **32-bit (x86)** version of [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader).
2. Extract the downloaded file and rename it to `dinput8.dll`.
3. Download the latest release of it.
4. Place `dinput8.dll`, the `.asi` plugin downloaded from here directly into your Dead Space game folder (where `Dead Space.exe` is).
5. Launch the game.
