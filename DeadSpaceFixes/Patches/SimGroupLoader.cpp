#include "common.h"
#include "Utils.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Features {

	namespace Modding {

		namespace SimGroupLoader {

			static constexpr const char* kTag = "[Features/Modding/SimGroupLoader]";

			const char* const kModsFolder = "simgroup_mods";
			const char* const kExtension = ".sgp";

			//a SimGroup is one blob with every pointer in it stored as an offset from its start,
			//same layout as Godfather 2's SimGroupTOC
			const uint32_t kSimGroupMagic = 0x53696D47; //'SimG'
			const uint32_t kSimGroupVersion = 1;
			const int kGroupMagic = 0x08;
			const int kGroupVersion = 0x0C;
			const int kGroupGuid = 0x10;
			const int kGroupPacketCount = 0x18;
			const int kGroupPackets = 0x28; //offset to the CAttributePacket* array, each entry is an offset as well
			const int kGroupHeaderSize = 0x30;

			//CAttributePacket
			const int kPacketFlags = 0x1C;      //bit 0 is compact
			const int kPacketFirstChunk = 0x20; //non compact packets are a run of {size, type, data...} chunks
			const int kPacketCompactId = 0x24;  //compact packets keep a self relative offset to their instance id here
			const int kPacketMinSize = 0x28;
			const uint32_t kInstanceIdChunk = 0x40000000;

			const int kLoadInfoData = 0x10; //CResourceLoadInfo's raw data, one field earlier than GF2's

			typedef std::array<uint32_t, 4> Guid;

			typedef void*(__thiscall* LoadResource_t)(void* pThis, BYTE* pLoadInfo);

			LoadResource_t oLoadResource = nullptr;

			//the override packets point into these and the game holds onto them, so they're never freed
			std::vector<std::unique_ptr<BYTE[]>> g_Files;
			std::map<Guid, BYTE*> g_Overrides;

			//where SimManager reads a packet's instance id from, the compact case is sub_7398E0 and the chunk walk is inlined in its update.
			//pBegin/pEnd bound it for our own files, the game's packets get walked the same way the game walks them
			const uint32_t* GetInstanceId(const BYTE* pPacket, const BYTE* pBegin, const BYTE* pEnd)
			{
				if (pPacket[kPacketFlags] & 1)
				{
					int32_t offset = *reinterpret_cast<const int32_t*>(pPacket + kPacketCompactId);
					if (offset == 0)
						return nullptr; //the game hands these an all zero id

					const BYTE* pId = pPacket + kPacketCompactId + offset;
					if (pEnd && (pId < pBegin || pId + sizeof(Guid) > pEnd))
						return nullptr;
					return reinterpret_cast<const uint32_t*>(pId);
				}

				const BYTE* pChunk = pPacket + kPacketFirstChunk;
				while (!pEnd || pChunk + 8 <= pEnd)
				{
					const uint32_t* pHeader = reinterpret_cast<const uint32_t*>(pChunk);
					if (pHeader[1] == kInstanceIdChunk)
						return (pEnd && pChunk + 8 + sizeof(Guid) > pEnd) ? nullptr : pHeader + 2;

					//the game would spin here forever
					if (pHeader[0] < 8)
						return nullptr;
					pChunk += pHeader[0];
				}
				return nullptr;
			}

			Guid ToGuid(const uint32_t* pId)
			{
				return { pId[0], pId[1], pId[2], pId[3] };
			}

			std::string GetModsFolder()
			{
				char path[MAX_PATH];
				GetModuleFileNameA(nullptr, path, MAX_PATH);

				std::string exePath(path);
				std::string::size_type pos = exePath.find_last_of("\\/");
				if (pos != std::string::npos)
					return exePath.substr(0, pos) + "\\" + kModsFolder;
				return std::string(".\\") + kModsFolder;
			}

			//sorted so the load order doesn't depend on the file system, a later file wins when two override the same packet
			std::vector<std::string> FindFiles(const std::string& folder)
			{
				std::vector<std::string> names;

				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA((folder + "\\*" + kExtension).c_str(), &findData);
				if (hFind == INVALID_HANDLE_VALUE)
					return names;

				do
				{
					const char* pExtension = strrchr(findData.cFileName, '.');

					if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && pExtension && _stricmp(pExtension, ".sgp") == 0)
						names.push_back(findData.cFileName);

				}
				while (FindNextFileA(hFind, &findData));
				FindClose(hFind);

				std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
					return _stricmp(a.c_str(), b.c_str()) < 0;
				});
				return names;
			}

			void LoadFile(const std::string& folder, const std::string& name)
			{
				std::ifstream file(folder + "\\" + name, std::ios::binary);
				if (!file)
				{
					LOG_WARN(kTag, "Couldn't open %s", name.c_str());
					return;
				}
				std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

				if (bytes.size() < kGroupHeaderSize)
				{
					LOG_WARN(kTag, "%s is too small to be a SimGroup", name.c_str());
					return;
				}

				std::unique_ptr<BYTE[]> pData(new BYTE[bytes.size()]);
				memcpy(pData.get(), bytes.data(), bytes.size());

				BYTE* pGroup = pData.get();
				const BYTE* pEnd = pGroup + bytes.size();
				uint32_t size = static_cast<uint32_t>(bytes.size());

				if (*reinterpret_cast<uint32_t*>(pGroup + kGroupMagic) != kSimGroupMagic
					|| *reinterpret_cast<uint32_t*>(pGroup + kGroupVersion) != kSimGroupVersion)
				{
					LOG_WARN(kTag, "%s isn't a version 1 SimGroup", name.c_str());
					return;
				}

				uint32_t count = *reinterpret_cast<uint32_t*>(pGroup + kGroupPacketCount);
				uint32_t arrayOffset = *reinterpret_cast<uint32_t*>(pGroup + kGroupPackets);
				if (arrayOffset == 0 || arrayOffset > size || count > (size - arrayOffset) / sizeof(uint32_t))
				{
					LOG_WARN(kTag, "%s has its packet table outside the file", name.c_str());
					return;
				}

				//the game never sees this group, only its packets, so the offsets just get read rather than fixed up in place
				const uint32_t* pPackets = reinterpret_cast<const uint32_t*>(pGroup + arrayOffset);
				int mounted = 0;
				for (uint32_t i = 0; i < count; i++)
				{
					uint32_t packetOffset = pPackets[i];
					if (packetOffset == 0 || packetOffset > size - kPacketMinSize)
					{
						LOG_WARN(kTag, "%s packet %u is outside the file, skipping it", name.c_str(), i);
						continue;
					}

					BYTE* pPacket = pGroup + packetOffset;
					const uint32_t* pId = GetInstanceId(pPacket, pGroup, pEnd);
					if (!pId)
					{
						LOG_WARN(kTag, "%s packet %u has no instance id, skipping it", name.c_str(), i);
						continue;
					}

					Guid id = ToGuid(pId);
					if (g_Overrides.count(id))
						LOG_INFO(kTag, "%s replaces an earlier override of %08X-%08X-%08X-%08X", name.c_str(), id[0], id[1], id[2], id[3]);
					g_Overrides[id] = pPacket;
					mounted++;

					LOG_DEBUG(kTag, "%s overrides %08X-%08X-%08X-%08X", name.c_str(), id[0], id[1], id[2], id[3]);
				}

				LOG_INFO(kTag, "Mounted %d of %u packet(s) from %s", mounted, count, name.c_str());
				if (mounted > 0)
					g_Files.push_back(std::move(pData));
			}

			//still in offset form here, pointing an entry at one of our packets means storing how far it is from the group
			void ApplyOverrides(BYTE* pGroup)
			{
				uint32_t arrayOffset = *reinterpret_cast<uint32_t*>(pGroup + kGroupPackets);
				if (arrayOffset == 0)
					return;

				uint32_t* pPackets = reinterpret_cast<uint32_t*>(pGroup + arrayOffset);
				uint32_t count = *reinterpret_cast<uint32_t*>(pGroup + kGroupPacketCount);
				int swapped = 0;
				for (uint32_t i = 0; i < count; i++)
				{
					if (pPackets[i] == 0)
						continue;

					const uint32_t* pId = GetInstanceId(pGroup + pPackets[i], nullptr, nullptr);
					if (!pId)
						continue;

					auto found = g_Overrides.find(ToGuid(pId));
					if (found == g_Overrides.end())
						continue;

					//32 bit so this wraps if ours is below the group, LoadResource adding the group back on wraps it right back
					pPackets[i] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(found->second) - reinterpret_cast<uintptr_t>(pGroup));
					swapped++;

					LOG_DEBUG(kTag, "Swapped in %08X-%08X-%08X-%08X", pId[0], pId[1], pId[2], pId[3]);
				}

				if (swapped > 0)
					LOG_INFO(kTag, "Swapped %d packet(s) in SimGroup %08X", swapped, *reinterpret_cast<uint32_t*>(pGroup + kGroupGuid));
			}

			//SimManager's resource handler, every SimGroup a stream loads comes through here (sub_6F5D20).
			//It turns the packet offsets into pointers and queues the group to spawn over the next few frames,
			//so swapping the offsets before it runs means none of that has to be redone like GF2ASI does
			void* __fastcall hkLoadResource(void* pThis, void* /*edx*/, BYTE* pLoadInfo)
			{
				BYTE* pGroup = *reinterpret_cast<BYTE**>(pLoadInfo + kLoadInfoData);
				if (pGroup
					&& *reinterpret_cast<uint32_t*>(pGroup + kGroupMagic) == kSimGroupMagic
					&& *reinterpret_cast<uint32_t*>(pGroup + kGroupVersion) == kSimGroupVersion)
					ApplyOverrides(pGroup);

				return oLoadResource(pThis, pLoadInfo);
			}

			void Apply(HMODULE hExe)
			{
				std::string folder = GetModsFolder();
				std::vector<std::string> names = FindFiles(folder);
				if (names.empty())
				{
					LOG_INFO(kTag, "No %s files in %s, not hooking anything", kExtension, folder.c_str());
					return;
				}

				for (const std::string& name : names)
					LoadFile(folder, name);

				if (g_Overrides.empty())
				{
					LOG_WARN(kTag, "None of the files in %s had a usable packet", folder.c_str());
					return;
				}

				//up to the 'SimG' check, cmp dword ptr [esi+8], 53696D47h
				uintptr_t loadResource = Utils::FindPattern(hExe, "53 55 8B 2D ? ? ? ? 56 57 8B F9 8B 8D 88 00 00 00 33 C0 85 C9 74 05 8B 59 18 EB 02 33 DB 8B 4C 24 14 8B 71 10 89 71 18 81 7E 08 47 6D 69 53");
				if (!loadResource)
				{
					LOG_WARN(kTag, "Couldn't find SimManager::LoadResource, not loading SimGroup overrides");
					return;
				}

				//the overrides are all in place before this, so the hook only ever reads them
				void* pLoadResource = reinterpret_cast<void*>(loadResource);
				if (MH_CreateHook(pLoadResource, &hkLoadResource, reinterpret_cast<void**>(&oLoadResource)) != MH_OK
					|| MH_EnableHook(pLoadResource) != MH_OK)
				{
					LOG_WARN(kTag, "Couldn't hook SimManager::LoadResource");
					return;
				}

				LOG_INFO(kTag, "Overriding %d packet(s) from %d file(s) in %s", static_cast<int>(g_Overrides.size()), static_cast<int>(g_Files.size()), folder.c_str());
			}
		}
	}
}
 