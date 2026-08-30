#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Tabs/PluginBrowserPanels.h"

#include "Input/GamepadInput.h"

#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/ActionFeedback.h"

#include <imgui.h>

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
        const float clearBtnWidth = ImGui::CalcTextSize(clearLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float searchFieldWidth = ImGui::GetContentRegionAvail().x * 0.55f - clearBtnWidth - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(searchFieldWidth);
        if (context.state.focusPending && !ImGui::IsAnyItemActive() && !GamepadInput::IsUsingGamepad()) {
            ImGui::SetKeyboardFocusHere();
            context.state.focusPending = false;
        }
        if (ImGui::InputTextWithHint("##PluginSearchInput", context.view.records.localize("PluginBrowser", "sSearch", "Plugin Search"), context.state.searchBuffer.data(), context.state.searchBuffer.size())) {
            context.state.search = context.state.searchBuffer.data();
        }
        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && context.state.searchBuffer[0] != '\0') {
            context.state.searchBuffer[0] = '\0';
            context.state.search.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string(clearLabel) + "###PluginSearchClear").c_str())) {
            context.state.searchBuffer[0] = '\0';
            context.state.search.clear();
        }
        ImGui::SameLine();

        if (ImGui::Button(context.view.records.localize("PluginBrowser", "sClearFilter", "Clear Plugin Filter"))) {
            context.requests.pluginFilter = std::string{};
        }

        if (ImGui::Checkbox(context.view.records.localize("PluginBrowser", "sGlobalSearch", "Global Search"), &context.state.globalSearch)) {
            context.requests.settingsChanged = true;
        }
        {
            auto wrappedSameLine = [](const char* label) {
                float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().ItemSpacing.x;
                if (ImGui::GetContentRegionAvail().x >= w) {
                    ImGui::SameLine();
                }
            };
            const char* unknownLabel = context.view.records.localize("PluginBrowser", "sShowUnknownCategories", "Show Unknown Categories");
            wrappedSameLine(unknownLabel);
            if (ImGui::Checkbox(unknownLabel, &context.state.showUnknown)) {
                listFilterSettingsChanged = true;
                context.requests.settingsChanged = true;
            }
        }

        if (!context.view.records.pluginFilter.empty()) {
            const std::string activePluginLabel = BuildPluginDisplayName(context.view.records.pluginFilter, plugins);
            ImGui::Text("%s: %s", context.view.records.localize("PluginBrowser", "sActiveFilter", "Active"), activePluginLabel.c_str());
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
        const float minDetailsWidth = 520.0f;
        const float preferredLeftWidth = totalWidth * 0.52f;
        const float maxLeftWidth = (std::max)(220.0f, totalWidth - minDetailsWidth - ImGui::GetStyle().ItemSpacing.x);
        const float leftWidth = (std::clamp)(preferredLeftWidth, 220.0f, maxLeftWidth);
        DrawTreePane(plugins, cache, context, leftWidth);

        ImGui::SameLine();

        DrawDetailsPane(plugins, cache, context);
    }
}
