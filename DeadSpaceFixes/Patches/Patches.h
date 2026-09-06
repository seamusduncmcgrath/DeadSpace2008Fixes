#pragma once

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
