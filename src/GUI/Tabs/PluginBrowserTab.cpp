#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Tabs/PluginBrowserPanels.h"

#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"

#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/ActionFeedback.h"

#include <imgui.h>
#include <limits>

namespace ESPExplorerAE
{
    using namespace PluginBrowserHelpers;
    using namespace PluginBrowserPanels;

    void PluginBrowserTab::Draw(PluginBrowserState& state, const PluginBrowserView& view, PluginBrowserRequests& requests)
    {
        if (!view.records.catalog) return;
        Context context{ state, view, requests };
        const auto& snapshot = view.records.catalog;
        ActionFeedback::Draw(state.admission, view.records.localize);
        const auto& cache = *snapshot;
        const auto& plugins = snapshot->plugins;
        bool listFilterSettingsChanged = false;

        const auto* clearLabel = view.records.localize("General", "sClearSearchButton", "X");
        SearchBar::Draw(view.records.localize("PluginBrowser", "sSearch", "Search name, FormID, EditorID, plugin or type"), state.searchBuffer.data(), state.searchBuffer.size(),
            state.search, &state.focusPending, "PluginSearchInput", clearLabel);

        if (ImGui::Checkbox(context.view.records.localize("PluginBrowser", "sGlobalSearch", "Global Search"), &context.state.globalSearch)) {
            context.requests.settingsChanged = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", view.records.localize("PluginBrowser", "sGlobalSearchHint",
            "Search all visible plugins, ignoring the active plugin filter. Record filters still apply."));
        {
            const char* unknownLabel = context.view.records.localize("PluginBrowser", "sShowUnknownCategories", "Show Unknown Categories");
            ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(unknownLabel).x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Checkbox(unknownLabel, &context.state.showUnknown)) {
                listFilterSettingsChanged = true;
                context.requests.settingsChanged = true;
            }
        }

        if (!context.view.records.pluginFilter.empty()) {
            const std::string activePluginLabel = BuildPluginDisplayName(context.view.records.pluginFilter, plugins);
            const auto filterLabel = activePluginLabel + "  " + clearLabel + "###PluginFilter";
            bool first = false;
            if (state.globalSearch) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGuiWidgetUtils::DrawWrappedButton(filterLabel.c_str(), first)) context.requests.pluginFilter = std::string{};
            if (state.globalSearch) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(activePluginLabel.c_str());
                ImGui::TextUnformatted(view.records.localize("PluginBrowser", "sClearFilter", "Clear Plugin Filter"));
                if (state.globalSearch) ImGui::TextUnformatted(view.records.localize("PluginBrowser", "sFilterBypassed", "Global Search is ignoring this plugin filter."));
                ImGui::EndTooltip();
            }
        }

        if (RecordFiltersWidget::Draw(
                context.view.records.localize,
                "PluginBrowser",
                RecordFilterState{
                    .showNonPlayable = context.view.records.filters.showNonPlayableRecords,
                    .showUnnamed = context.view.records.filters.showUnnamedRecords,
                    .showDeleted = context.view.records.filters.showDeletedRecords,
                    .advancedRules = context.view.records.filters.advancedRecordFilters,
                    .hiddenPlugins = context.view.records.filters.hiddenPlugins }, context.state.filterEditor, context.view.records.catalog)) {
            listFilterSettingsChanged = true;
        }

        if (listFilterSettingsChanged) {
            ++context.view.records.filters.advancedRecordFilterRevision;
            context.requests.records.filtersChanged = true;
        }

        CatalogQuery filter;
        filter.plugin = context.requests.pluginFilter ? *context.requests.pluginFilter : std::string(context.view.records.pluginFilter);
        filter.search = context.state.search;
        filter.showPlayable = context.view.records.filters.showPlayableRecords;
        filter.showNonPlayable = context.view.records.filters.showNonPlayableRecords;
        filter.showNamed = context.view.records.filters.showNamedRecords;
        filter.showUnnamed = context.view.records.filters.showUnnamedRecords;
        filter.showDeleted = context.view.records.filters.showDeletedRecords;
        filter.hiddenPlugins = context.view.records.filters.hiddenPlugins;
        const auto& result = context.state.query.Update(snapshot, std::move(filter), context.view.records.filters.advancedRecordFilters,
            context.view.records.filters.advancedRecordFilterRevision, context.state.showUnknown, context.state.globalSearch);
        context.state.selection.Reconcile(result.eligibleIDs);

        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float minLeftWidth = (std::min)(ImGui::GetFontSize() * 12.0f, totalWidth * 0.35f);
        const float minDetailsWidth = (std::min)(ImGui::GetFontSize() * 26.0f, totalWidth * 0.60f);
        const float maxLeftWidth = (std::max)(minLeftWidth, totalWidth - minDetailsWidth - spacing);
        const float leftWidth = (std::clamp)(totalWidth * 0.44f, minLeftWidth, maxLeftWidth);
        // Constrain the draggable tree edge so it cannot squeeze actions out of
        // the details pane; the limits also adapt when the menu/font size changes.
        ImGui::SetNextWindowSizeConstraints(ImVec2(minLeftWidth, 0.0f), ImVec2(maxLeftWidth, (std::numeric_limits<float>::max)()));
        DrawTreePane(plugins, cache, context, leftWidth);
        ImGui::SameLine();
        DrawDetailsPane(plugins, cache, context);
    }
}
