#pragma once

#include <Windows.h>

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
