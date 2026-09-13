#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Tabs/PluginBrowserPanels.h"

#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SearchControls.h"
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

        DrawSearchControls(state.structuredSearch, state.scope, state.search, state.searchBuffer.data(), state.searchBuffer.size(), *snapshot, view.records.localize);
        ImGui::BeginDisabled(state.allRuntimeRecords);
        {
            const char* unknownLabel = context.view.records.localize("PluginBrowser", "sShowUnknownCategories", "Show Unknown Categories");
            ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(unknownLabel).x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
            if (ImGui::Checkbox(unknownLabel, &context.state.showUnknown)) {
                listFilterSettingsChanged = true;
                context.requests.settingsChanged = true;
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
        ImGui::EndDisabled();

        CatalogQuery filter;
        filter.scope = state.scope;
        filter.structuredSearch = state.structuredSearch;
        filter.search = context.state.search;
        if (!state.allRuntimeRecords) RecordFiltersWidget::DrawWhyHidden(view.records.localize, "PluginBrowser", {view.records.filters.showNonPlayableRecords, view.records.filters.showUnnamedRecords,
            view.records.filters.showDeletedRecords, view.records.filters.advancedRecordFilters, view.records.filters.hiddenPlugins}, state.filterEditor, filter, snapshot);
        const auto* allLabel = view.records.localize("PluginBrowser", "sAllRuntimeRecords", "All runtime records");
        ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(allLabel).x + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::Checkbox(allLabel, &state.allRuntimeRecords);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", view.records.localize("PluginBrowser", "sAllRuntimeRecordsHint",
            "Shows hidden and excluded records here. Search and scope still apply."));
        filter.showPlayable = context.view.records.filters.showPlayableRecords;
        filter.showNonPlayable = context.view.records.filters.showNonPlayableRecords;
        filter.showNamed = context.view.records.filters.showNamedRecords;
        filter.showUnnamed = context.view.records.filters.showUnnamedRecords;
        filter.showDeleted = context.view.records.filters.showDeletedRecords;
        filter.hiddenPlugins = context.view.records.filters.hiddenPlugins;
        const auto& result = context.state.query.Update(snapshot, std::move(filter), context.view.records.filters.advancedRecordFilters,
            context.view.records.filters.advancedRecordFilterRevision, context.state.showUnknown, context.state.globalSearch, state.allRuntimeRecords);
        context.state.selection.Reconcile(result.eligibleIDs);

        const float totalWidth = ImGui::GetContentRegionAvail().x;
        if (!view.showInspector) { DrawTreePane(plugins, cache, context, totalWidth); return; }
        const float spacing = ImGuiWidgetUtils::PaneDividerSize();
        const float minLeftWidth = (std::min)(ImGui::GetFontSize() * 12.0f, totalWidth * 0.35f);
        const float minDetailsWidth = (std::min)(ImGui::GetFontSize() * (view.drawInspector ? 20.0f : 26.0f), totalWidth * 0.60f);
        const float maxLeftWidth = (std::max)(minLeftWidth, totalWidth - minDetailsWidth - spacing);
        float leftWidth = (std::clamp)(view.drawInspector ? totalWidth - view.inspectorWidth - spacing :
            state.resultsWidth > 0 ? state.resultsWidth : totalWidth * 0.44f, minLeftWidth, maxLeftWidth);
        DrawTreePane(plugins, cache, context, leftWidth);
        ImGui::SameLine(0, 0);
        ImGuiWidgetUtils::PaneDivider("##PluginResultsDivider", leftWidth, minLeftWidth, maxLeftWidth,
            view.records.localize("General", "sResizePanes", "Drag to resize panes"));
        state.resultsWidth = leftWidth;
        requests.resultsWidth = leftWidth;
        ImGui::SameLine(0, 0);
        DrawDetailsPane(plugins, cache, context);
    }
}
