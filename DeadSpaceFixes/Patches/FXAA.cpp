#include "common.h"
#include "Utils.h"
#include "Config.h"

#include "Shaders/FXAA_EdgeAAFast.h"
#include "Shaders/FXAA_DeferredEdgeAA.h"

#include <cstring>

//Swaps the game's EdgeAA pixel shaders for FXAA (Shaders/FXAA.hlsl) and makes the pass run every frame.
//
//The EdgeAA pass (sub_695350) copies the finished, colour graded frame into a texture and draws a fullscreen
//quad back with one of two shaders: EdgeAAFast, a basic luma-weighted blur, or DeferredEdgeAA (depth + normals).
//The game creates both from shader bytecode tables in its data and binds their parameters by name, so pointing
//the tables at FXAA bytecode with matching parameter names is all it takes. That has to happen before the
//game creates its shaders, which is why this runs from DllMain.
//
//Vanilla only runs the pass on frames where the scene's effect settings ask for an edge blur, and can fold it
//into a combined post pass, so the gate is forced open and the combining check forced off.
//The in-game EdgeAA option still switches it on and off.
namespace Features {

	namespace Graphics {

		namespace FXAA {

			namespace {

				const char* const kTag = "[Features/Graphics/FXAA]";

				//EdgeAAShader init (sub_6A0AE0) creating its shaders:
				//mov ecx, offset vsTable; call CreateVS; mov ecx, offset deferredEdgeAATable; mov [esi+10h], eax; call CreatePS;
				//mov [esi+14h], eax; mov eax, esi; call ...; mov eax, [esi+14h]; mov ecx, offset edgeAAFastTable; mov [esi+100h], eax
				const char* const kShaderTablesSignature = "B9 ? ? ? ? E8 ? ? ? ? B9 ? ? ? ? 89 46 10 E8 ? ? ? ? 89 46 14 8B C6 E8 ? ? ? ? 8B 46 14 B9 ? ? ? ? 89 86 00 01 00 00";
				const size_t kDeferredTableOffset = 11;
				const size_t kFastTableOffset = 37;
				//one entry per depth readback mode (dword_CCE760, 0..3), the game picks one when it creates the shader
				const int kTableEntries = 4;

				//sub_695640, only runs the pass when this frame's effect settings ask for it:
				//movss xmm0, [1/127]; comiss xmm0, [strengthA]; ja +9; comiss xmm0, [strengthB]; jbe +0Ch; test [flags], 400000h; jz skip
				const char* const kGateSignature = "F3 0F 10 05 ? ? ? ? 0F 2F 05 ? ? ? ? 77 09 0F 2F 05 ? ? ? ? 76 0C F7 05 ? ? ? ? 00 00 40 00 74";
				//jmp short straight to the call to the pass, 0x26 bytes in
				const BYTE kGatePatch[] = { 0xEB, 0x24 };

				//sub_695600, tells the post chain whether EdgeAA was asked for so a combined pass can take it over
				const char* const kComboSignature = "F3 0F 10 05 ? ? ? ? 0F 2F 05 ? ? ? ? A1 ? ? ? ? 77 09 0F 2F 05 ? ? ? ? 76 07 A9 00 00 40 00";
				//xor eax, eax; ret
				const BYTE kComboPatch[] = { 0x31, 0xC0, 0xC3 };

				//the game hands these straight to CreatePixelShader and D3DXGetShaderConstantTable, so keep DWORD aligned copies
				alignas(16) BYTE g_FastBytecode[sizeof(g_FxaaEdgeAAFastPS)];
				alignas(16) BYTE g_DeferredBytecode[sizeof(g_FxaaDeferredEdgeAAPS)];

				//DllMain runs before the console exists, so ApplyStartup records what happened and Apply logs it
				enum class Status { NotRun, Applied, MissingPattern, ShaderSwapFailed, GatePatchFailed };
				Status g_Status = Status::NotRun;

				bool PointTableAt(uintptr_t table, const void* bytecode)
				{
					const void* entries[kTableEntries];
					for (const void*& entry : entries)
						entry = bytecode;
					return Utils::WriteBytes(table, entries, sizeof(entries));
				}
			}

			void ApplyStartup()
			{
				if (!Config::Features::UseFXAA)
					return;

				HMODULE hExe = GetModuleHandleA(nullptr);
				uintptr_t shaderTables = Utils::FindPattern(hExe, kShaderTablesSignature);
				uintptr_t gate = Utils::FindPattern(hExe, kGateSignature);
				uintptr_t combo = Utils::FindPattern(hExe, kComboSignature);

				//all or nothing, forcing the pass on with the game's own blur still in it would blur every frame
				if (!shaderTables || !gate || !combo)
				{
					g_Status = Status::MissingPattern;
					return;
				}

				memcpy(g_FastBytecode, g_FxaaEdgeAAFastPS, sizeof(g_FastBytecode));
				memcpy(g_DeferredBytecode, g_FxaaDeferredEdgeAAPS, sizeof(g_DeferredBytecode));

				uintptr_t deferredTable = *reinterpret_cast<uintptr_t*>(shaderTables + kDeferredTableOffset);
				uintptr_t fastTable = *reinterpret_cast<uintptr_t*>(shaderTables + kFastTableOffset);

				if (!PointTableAt(fastTable, g_FastBytecode) || !PointTableAt(deferredTable, g_DeferredBytecode))
				{
					g_Status = Status::ShaderSwapFailed;
					return;
				}

				//shaders are swapped, so from here on the worst case is FXAA only running when vanilla EdgeAA would have
				if (!Utils::WriteBytes(combo, kComboPatch, sizeof(kComboPatch)) || !Utils::WriteBytes(gate, kGatePatch, sizeof(kGatePatch)))
				{
					g_Status = Status::GatePatchFailed;
					return;
				}

				g_Status = Status::Applied;
			}

			void Apply(HMODULE)
			{
				if (!Config::Features::UseFXAA)
					return;

				switch (g_Status)
				{
				case Status::Applied:
					LOG_INFO(kTag, "EdgeAA now runs FXAA every frame (toggle it with the in-game Edge AA option)");
					break;
				case Status::MissingPattern:
					LOG_WARN(kTag, "Couldn't find the EdgeAA shader tables or pass, FXAA not applied");
					break;
				case Status::ShaderSwapFailed:
					LOG_WARN(kTag, "Couldn't swap the EdgeAA shaders, FXAA not applied");
					break;
				case Status::GatePatchFailed:
					LOG_WARN(kTag, "EdgeAA shaders swapped for FXAA, but it only runs when the game asks for edge AA");
					break;
				case Status::NotRun:
					LOG_WARN(kTag, "ApplyStartup wasn't called from DllMain, FXAA not applied (it has to run before the game creates its shaders)");
					break;
				}
			}
		}
	}
}
