#pragma once

#include "Core/FormEntry.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ESPExplorerAE
{
    struct PluginInfo
    {
        std::string filename;
        std::string type;
        std::uint32_t loadOrder;
        std::uint32_t lightOrder;
        bool isLight;
        std::string formIDPrefix;
        std::uint32_t runtimeSourceRecords{};
        std::uint32_t runtimeOriginRecords{};
        std::optional<std::uint32_t> overrideCount;
        std::optional<std::uint32_t> overriddenByOthersCount;
        std::vector<std::string> masters{};
        std::vector<std::string> missingMasters{};
    };

    using RecordIndex = std::size_t;

    struct CatalogSnapshot
    {
        std::uint64_t generation{};
        bool ready{};
        std::vector<FormEntry> records;
        std::vector<PluginInfo> plugins;
        std::unordered_map<std::uint32_t, RecordIndex> byID;
        std::unordered_map<std::string, std::vector<RecordIndex>> byType;
        std::unordered_map<std::string, std::vector<RecordIndex>> byPlugin;
        std::unordered_map<std::uint32_t, std::uint32_t> runtimeReferenceCounts;
        std::vector<std::string> availableKeywords;

        const FormEntry* Find(std::uint32_t id) const
        {
            const auto found = byID.find(id);
            return found == byID.end() ? nullptr : &records[found->second];
        }

        // Detached construction only, before immutable publication. Never apply
        // user visibility preferences here: hidden records remain discoverable.
        void BuildIndexes()
        {
            byID.clear();
            byType.clear();
            byPlugin.clear();
            std::ranges::sort(records, {}, &FormEntry::formID);
            for (RecordIndex i = 0; i < records.size(); ++i) {
                const auto& record = records[i];
                if (!record.formID || !byID.emplace(record.formID, i).second) throw std::invalid_argument("catalog identity must be nonzero and unique");
                byType[record.category].push_back(i);
                byPlugin[record.sourcePlugin].push_back(i);
            }
            std::ranges::sort(availableKeywords);
            availableKeywords.erase(std::unique(availableKeywords.begin(), availableKeywords.end()), availableKeywords.end());
        }
    };

    struct CatalogResult
    {
        std::shared_ptr<const CatalogSnapshot> snapshot;
        std::uint64_t revision{};
        std::vector<RecordIndex> order;

        const FormEntry& At(std::size_t row) const { return snapshot->records.at(order.at(row)); }
    };
}
