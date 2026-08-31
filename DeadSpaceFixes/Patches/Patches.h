#pragma once

// Behaviour-modifying patches: change how existing game features behave without
// adding brand-new capability. Each patch is a self-contained .cpp file (listed
// in the project) exposing a single `Apply(HMODULE)` entry point declared here.

#include <Windows.h>

namespace Patches {

	namespace Gameplay {
		namespace IntroCutscene {
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
