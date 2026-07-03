#pragma once

#include "Core/BrowserQuery.h"
#include <map>

namespace ESPExplorerAE
{
    struct PluginQueryResult
    {
        CatalogResult records;
        std::map<std::string, std::map<std::string, std::vector<RecordIndex>>> groups;
        std::vector<std::string> plugins;
        std::unordered_set<std::uint32_t> eligibleIDs;
    };

    class PluginQuery
    {
    public:
        const PluginQueryResult& Update(std::shared_ptr<const CatalogSnapshot> snapshot, CatalogQuery filter,
            std::span<const AdvancedFilterRule> rules, std::uint64_t filterRevision, bool showUnknown, bool globalSearch)
        {
            // Favorites/recent records deliberately ignore the cross-tab plugin
            // filter. They share visibility/search rules with the tree.
            const auto plugin = filter.plugin;
            filter.plugin.clear();
            filter.sortColumn = 0;
            filter.searchNPCMetadata = false;
            const auto& eligible = query.Update(std::move(snapshot), std::move(filter), rules, filterRevision);
            if (sourceRevision == eligible.revision && selectedPlugin == plugin && unknown == showUnknown && global == globalSearch) return result;
            const ProfileScope profileScope(ProfileMetric::PluginGrouping);
            PluginQueryResult next;
            next.records = { eligible.snapshot, ++revision, {} };
            for (const auto index : eligible.order) {
                const auto& record = eligible.snapshot->records[index];
                if (!showUnknown && (record.sourcePlugin.empty() || record.category.empty() ||
                    TextEquals(record.category, "unknown") || TextEquals(record.category, "<unknown>"))) continue;
                next.records.order.push_back(index);
                next.eligibleIDs.insert(record.formID);
                if (globalSearch || plugin.empty() || plugin == record.sourcePlugin) next.groups[record.sourcePlugin][record.category].push_back(index);
            }
            std::unordered_set<std::string> seen;
            if (eligible.snapshot) for (const auto& info : eligible.snapshot->plugins) {
                if (next.groups.contains(info.filename)) { next.plugins.push_back(info.filename); seen.insert(info.filename); }
            }
            // Unknown/runtime-only origins follow load-order plugins in stable
            // byte order. The empty origin stays an identity, never a UI label.
            for (const auto& [name, groups] : next.groups) if (!seen.contains(name)) next.plugins.push_back(name);
            result = std::move(next);
            sourceRevision = eligible.revision;
            selectedPlugin = plugin;
            unknown = showUnknown;
            global = globalSearch;
            return result;
        }
        std::size_t IndexBytes() const
        {
            auto bytes = query.IndexBytes() + result.records.order.capacity() * sizeof(RecordIndex);
            for (const auto& [plugin, groups] : result.groups) for (const auto& [category, rows] : groups) bytes += rows.capacity() * sizeof(RecordIndex);
            return bytes;
        }
        const PluginQueryResult& Result() const { return result; }
        void Clear() { query.Clear(); sourceRevision = 0; result = {}; }

    private:
        BrowserQuery query;
        PluginQueryResult result;
        std::uint64_t sourceRevision{};
        std::uint64_t revision{};
        std::string selectedPlugin;
        bool unknown{};
        bool global{};
    };
}
