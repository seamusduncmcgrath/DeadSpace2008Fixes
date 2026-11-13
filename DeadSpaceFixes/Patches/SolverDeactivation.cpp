#include "common.h"
#include "Utils.h"

#include "Config.h"

#include <algorithm>

namespace Fixes {

	namespace Physics {

		namespace SolverDeactivation {

			static constexpr const char* kTag = "[Fixes/Physics/SolverDeactivation]";

			const float kReferenceStep = 1.0f / 30.0f;

			//hkpSolverInfo::DeactivationInfo, one per deactivation class
			const int kDeactivationClasses = 6;
			const int kDeactivationInfoSize = 0x1C;
			const int kSlowVelocityMultiplier = 0x8;
			const int kSleepVelocityThreshold = 0xC;

			void* oPhysicsStep = nullptr;
			int g_WorldOffset = 0;            //the Havok world in the physics manager
			int g_DeactivationInfoOffset = 0; //the deactivation info in the world

			BYTE* g_pWorld = nullptr;
			float g_GameMultiplier[kDeactivationClasses] = {};
			float g_GameSleepThreshold[kDeactivationClasses] = {};
			float g_WrittenMultiplier[kDeactivationClasses] = {};
			float g_WrittenSleepThreshold[kDeactivationClasses] = {};

			float* DeactivationField(BYTE* pWorld, int deactivationClass, int field)
			{
				return reinterpret_cast<float*>(pWorld + g_DeactivationInfoOffset + deactivationClass * kDeactivationInfoSize + field);
			}

			void __stdcall OnPhysicsStep(BYTE* pManager, float dt)
			{
				BYTE* pWorld = *reinterpret_cast<BYTE**>(pManager + g_WorldOffset);
				if (!pWorld)
					return;

				//a new world (or something else changed the values), so what's there now is the game's
				bool fresh = pWorld != g_pWorld;
				for (int i = 0; i < kDeactivationClasses && !fresh; i++)
				{
					fresh = *DeactivationField(pWorld, i, kSlowVelocityMultiplier) != g_WrittenMultiplier[i] || *DeactivationField(pWorld, i, kSleepVelocityThreshold) != g_WrittenSleepThreshold[i];
				}

				if (fresh)
				{
					for (int i = 0; i < kDeactivationClasses; i++)
					{
						g_GameMultiplier[i] = *DeactivationField(pWorld, i, kSlowVelocityMultiplier);
						g_GameSleepThreshold[i] = *DeactivationField(pWorld, i, kSleepVelocityThreshold);
						g_WrittenMultiplier[i] = g_GameMultiplier[i];
						g_WrittenSleepThreshold[i] = g_GameSleepThreshold[i];
					}
					g_pWorld = pWorld;
					LOG_DEBUG(kTag, "New physics world at 0x%X, slow velocity multipliers %.4f / %.4f / %.4f / %.4f", reinterpret_cast<uintptr_t>(pWorld),
						g_GameMultiplier[2], g_GameMultiplier[3], g_GameMultiplier[4], g_GameMultiplier[5]);
				}

				float scale = (std::clamp)(dt / kReferenceStep, 0.0f, 1.0f);

				for (int i = 0; i < kDeactivationClasses; i++)
				{
					//the classes that never slow anything down (deactivation off) are left alone
					if (g_GameMultiplier[i] >= 1.0f)
						continue;

					g_WrittenMultiplier[i] = 1.0f - (1.0f - g_GameMultiplier[i]) * scale;
					g_WrittenSleepThreshold[i] = g_GameSleepThreshold[i] * scale;
					*DeactivationField(pWorld, i, kSlowVelocityMultiplier) = g_WrittenMultiplier[i];
					*DeactivationField(pWorld, i, kSleepVelocityThreshold) = g_WrittenSleepThreshold[i];
				}
			}

			//the physics managers step (sub_78EE50) the manager in EDI and the step time on the stack
			__declspec(naked) void hkPhysicsStep()
			{
				__asm
				{
					pushad
					push dword ptr [esp + 36] //the step time, past what pushad saved and the return address
					push edi
					call OnPhysicsStep
					popad
					jmp dword ptr [oPhysicsStep]
				}
			}

			void Apply(HMODULE hExe)
			{
				if (!Config::Fixes::HavokSolverFix)
					return;

				uintptr_t physicsStep = Utils::FindPattern(hExe, "80 7F 24 00 D9 44 24 04 56 51 D9 1C 24 C6 47 48 01 74 0A 8B 77 28 E8 ? ? ? ? EB 08 8B 4F 20 E8");
				
				uintptr_t deactivationSetup = Utils::FindPattern(hExe, "8D 86 ? ? ? ? D8 CB 8D 0C D5 00 00 00 00 2B CA 8D 4C 88 ?");
				if (!physicsStep || !deactivationSetup)
				{
					LOG_WARN(kTag, "Couldn't find the physics step or the Havok world's deactivation info, not patching");
					return;
				}

				g_WorldOffset = *reinterpret_cast<BYTE*>(physicsStep + 0x1F);
				g_DeactivationInfoOffset = *reinterpret_cast<int*>(deactivationSetup + 2) + *reinterpret_cast<BYTE*>(deactivationSetup + 20);

				void* pTarget = reinterpret_cast<void*>(physicsStep);
				if (MH_CreateHook(pTarget, &hkPhysicsStep, &oPhysicsStep) != MH_OK || MH_EnableHook(pTarget) != MH_OK)
				{
					LOG_WARN(kTag, "Couldn't hook the physics step at 0x%X", physicsStep);
					return;
				}

				LOG_INFO(kTag, "Hooked the physics step at 0x%X, slow objects now come to rest at the same rate at any frame rate", physicsStep);
			}
		}
	}
}
