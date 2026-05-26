#pragma once

#include "Core/CatalogSnapshot.h"
#include <optional>

namespace ESPExplorerAE
{
    enum class NPCFilterMode { kAll, kByRace, kByFaction, kEssential, kUnique, kProtected, kMaleOnly, kFemaleOnly };

    struct NPCQuery
    {
        NPCFilterMode mode{ NPCFilterMode::kAll };
        std::optional<std::string> selectedRace;
        std::optional<std::string> selectedFaction;
        bool operator==(const NPCQuery&) const = default;

        bool Matches(const FormEntry& entry) const
        {
            if (mode == NPCFilterMode::kAll) return true;
            if (!entry.hasNPCData) return false;
            switch (mode) {
            case NPCFilterMode::kByRace: return !selectedRace || entry.race == *selectedRace;
            case NPCFilterMode::kByFaction: return !selectedFaction || std::ranges::find(entry.factionNames, *selectedFaction) != entry.factionNames.end();
            case NPCFilterMode::kEssential: return entry.npcEssential;
            case NPCFilterMode::kUnique: return entry.npcUnique;
            case NPCFilterMode::kProtected: return entry.npcProtected;
            case NPCFilterMode::kMaleOnly: return !entry.npcFemale;
            case NPCFilterMode::kFemaleOnly: return entry.npcFemale;
            default: return true;
            }
        }
    };

    struct NPCFacets
    {
        std::vector<std::string> races;
        std::unordered_map<std::string, std::size_t> raceCounts;
        std::vector<std::string> factions;
        std::unordered_map<std::string, std::size_t> factionCounts;

        static NPCFacets Build(const CatalogResult& result)
        {
            NPCFacets facets;
            for (std::size_t i = 0; i < result.order.size(); ++i) {
                const auto& entry = result.At(i);
                if (facets.raceCounts.try_emplace(entry.race, 0).second) facets.races.push_back(entry.race);
                ++facets.raceCounts[entry.race];
                for (const auto& faction : entry.factionNames) {
                    if (facets.factionCounts.try_emplace(faction, 0).second) facets.factions.push_back(faction);
                    ++facets.factionCounts[faction];
                }
            }
            std::ranges::sort(facets.races, [](const auto& left, const auto& right) {
                if (left.empty()) return false;
                return right.empty() || left < right;
            });
            std::ranges::sort(facets.factions);
            return facets;
        }
    };
}
