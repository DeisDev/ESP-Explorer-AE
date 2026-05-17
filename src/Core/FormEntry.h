#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ESPExplorerAE
{
    struct FormEntry
    {
        std::uint32_t formID{ 0 };
        std::string name;
        std::string category;
        std::string sourcePlugin; // Reported GetFile(0); not a proven effective override or durable origin key.
        std::string race;
        std::string factions;
        bool hasNPCData{ false };
        bool npcEssential{ false };
        bool npcUnique{ false };
        bool npcProtected{ false };
        bool npcFemale{ false };
        bool isPlayable{ true };
        bool isDeleted{ false };
        std::string editorID;
        std::vector<std::string> keywords;
        std::vector<std::string> factionNames;
        std::uint32_t weaponAmmoID{};
    };

}
