#pragma once

#include "GUI/Tabs/PluginBrowserTab.h"

#include <imgui.h>

namespace ESPExplorerAE::PluginBrowserHelpers
{
    // Frame-local rendering inputs; application commands leave through requests.
    struct Context
    {
        PluginBrowserState& state;
        const PluginBrowserView& view;
        PluginBrowserRequests& requests;
    };

    std::string BuildPluginDisplayName(std::string_view pluginName, const std::vector<PluginInfo>& plugins);
    std::string CategoryDisplayName(std::string_view category, const Context& context);
    ImVec4 CategoryColor(std::string_view category);
    const FormEntry* FindRecordByFormID(const CatalogSnapshot& cache, std::uint32_t formID);
    void TrackRecentRecord(std::uint32_t formID, Context& context);
    void EnsurePrimarySelectionValid(Context& context);
    std::vector<RecordIndex> CollectSelectedGiveableEntries(const CatalogSnapshot& cache, const Context& context);
    std::vector<RecordIndex> CollectSelectedEntries(const CatalogSnapshot& cache, const Context& context);
    void RequestGrant(std::span<const RecordIndex> records, Context& context);
    void DrawRecordContextMenu(const FormEntry& record, bool isSelected, const CatalogSnapshot& cache, Context& context);
}