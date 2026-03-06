#pragma once

#include "GUI/Tabs/PluginBrowserTab.h"

#include <imgui.h>

namespace ESPExplorerAE::PluginBrowserHelpers
{
    bool PassesLocalRecordFilters(const FormEntry& entry, const PluginBrowserTabContext& context);
    bool MatchesPluginSearch(const FormEntry& entry, std::string_view query, bool caseSensitive);
    bool IsUnknownCategory(std::string_view category);
    std::string BuildPluginDisplayName(std::string_view pluginName, const std::vector<PluginInfo>& plugins);
    std::string CategoryDisplayName(std::string_view category, const PluginBrowserTabContext& context);
    ImVec4 CategoryColor(std::string_view category);
    const FormEntry* FindRecordByFormID(const FormCache& cache, std::uint32_t formID, std::uint64_t dataVersion);
    void TrackRecentRecord(std::uint32_t formID, PluginBrowserTabContext& context);
    void EnsurePrimarySelectionValid(PluginBrowserTabContext& context);
    std::vector<FormEntry> CollectSelectedGiveableEntries(const FormCache& cache, std::uint64_t dataVersion, const PluginBrowserTabContext& context);
    std::vector<FormEntry> CollectSelectedEntries(const FormCache& cache, std::uint64_t dataVersion, const PluginBrowserTabContext& context);
    void EquipRecordWithConfiguredAmmo(const FormEntry& record, int ammoCount);
    void DrawRecordContextMenu(const FormEntry& record, bool isSelected, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context);
    void RebuildCachesIfNeeded(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context);
}