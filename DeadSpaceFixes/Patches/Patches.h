#pragma once

#include <Windows.h>

namespace Patches {

	namespace Gameplay {
		namespace IntroCutscene {
			void Apply(HMODULE hExe);
		}
		namespace SkipVideos {
			void Apply(HMODULE hExe);
		}
	}

	namespace UI {
		namespace MainIntro {
			void Apply(HMODULE hExe);
		}
		namespace VersionString {
			void Apply(HMODULE hExe);
		}
	}

	namespace System {
		namespace BorderlessWindow {
			void Apply();
		}
		namespace Telemetry {
			void Apply();
		}
	}

}

namespace Features {

	namespace Input {
		namespace SdlGamepad {
			void StartThread();
			void Shutdown();
		}
	}

	namespace Graphics {
		namespace AnisotropicFiltering {
			void Apply(IDirect3DDevice9* pDummyDevice);
		}
		namespace FrameRateCap {
			void Apply(IDirect3DDevice9* pDummyDevice);
		}
		namespace D3D9 {
			void InstallDeviceHooks();
		}
	}

}

namespace Fixes {

	namespace Physics {
		namespace Timer {
			void Apply(HMODULE hExe);
		}
	}

	namespace Graphics {
		namespace VSync {
			void ApplyStartup();
			void Apply(HMODULE hExe);
		}
		namespace SubtitleScale {
			void Apply(HMODULE hExe);
		}
	}

	namespace UI {
		namespace LoadingScreen {
			void Apply(HMODULE hExe);
		}
	}

	namespace Input {
		namespace LegacyDirectInput {
			void Apply(HMODULE hExe);
		}
	}

	namespace Save {
		namespace SafeStringHandling {
			void Apply(HMODULE hExe);
		}
	}

}
