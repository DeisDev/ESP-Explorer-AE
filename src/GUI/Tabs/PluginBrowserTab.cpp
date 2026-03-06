#include "GUI/Tabs/PluginBrowserTab.h"
#include "GUI/Tabs/PluginBrowserHelpers.h"
#include "GUI/Tabs/PluginBrowserPanels.h"

#include "GUI/Widgets/RecordFiltersWidget.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    using namespace PluginBrowserHelpers;
    using namespace PluginBrowserPanels;

    void PluginBrowserTab::Draw(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context)
    {
        bool listFilterSettingsChanged = false;

        const float clearBtnWidth = ImGui::CalcTextSize("X").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float searchFieldWidth = ImGui::GetContentRegionAvail().x * 0.55f - clearBtnWidth - ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetNextItemWidth(searchFieldWidth);
        if (context.searchFocusPending && *context.searchFocusPending && !ImGui::IsAnyItemActive() && !ImGui::GetIO().NavActive) {
            ImGui::SetKeyboardFocusHere();
            *context.searchFocusPending = false;
        }
        if (ImGui::InputTextWithHint("##PluginSearchInput", context.localize("PluginBrowser", "sSearch", "Plugin Search"), context.pluginSearchBuffer, context.pluginSearchBufferSize)) {
            context.pluginSearch = context.pluginSearchBuffer;
        }
        ImGui::SameLine();
        if (ImGui::Button("X##PluginSearchClear")) {
            context.pluginSearchBuffer[0] = '\0';
            context.pluginSearch.clear();
        }
        ImGui::SameLine();

        if (ImGui::Button(context.localize("PluginBrowser", "sClearFilter", "Clear Plugin Filter"))) {
            context.selectedPluginFilter.clear();
        }

        if (ImGui::Checkbox(context.localize("PluginBrowser", "sGlobalSearch", "Global Search"), &context.pluginGlobalSearchMode)) {
            context.persistFilterCheckboxes();
        }
        {
            auto wrappedSameLine = [](const char* label) {
                float w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFontSize() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetStyle().ItemSpacing.x;
                if (ImGui::GetContentRegionAvail().x >= w) {
                    ImGui::SameLine();
                }
            };
            const char* unknownLabel = context.localize("PluginBrowser", "sShowUnknownCategories", "Show Unknown Categories");
            wrappedSameLine(unknownLabel);
            if (ImGui::Checkbox(unknownLabel, &context.showUnknownCategories)) {
                listFilterSettingsChanged = true;
                context.persistFilterCheckboxes();
            }
        }

        if (!context.selectedPluginFilter.empty()) {
            const std::string activePluginLabel = BuildPluginDisplayName(context.selectedPluginFilter, plugins);
            ImGui::Text("%s: %s", context.localize("PluginBrowser", "sActiveFilter", "Active"), activePluginLabel.c_str());
        }

        if (RecordFiltersWidget::Draw(
                context.localize,
                "PluginBrowser",
                RecordFilterState{
                    .showNonPlayable = context.showNonPlayableRecords,
                    .showUnnamed = context.showUnnamedRecords,
                    .showDeleted = context.showDeletedRecords })) {
            listFilterSettingsChanged = true;
        }

        if (listFilterSettingsChanged) {
            context.persistListFilters();
        }

        RebuildCachesIfNeeded(plugins, cache, dataVersion, context);

        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float minDetailsWidth = 520.0f;
        const float preferredLeftWidth = totalWidth * 0.52f;
        const float maxLeftWidth = (std::max)(220.0f, totalWidth - minDetailsWidth - ImGui::GetStyle().ItemSpacing.x);
        const float leftWidth = (std::clamp)(preferredLeftWidth, 220.0f, maxLeftWidth);
        DrawTreePane(plugins, cache, dataVersion, context, leftWidth);

        ImGui::SameLine();

        DrawDetailsPane(cache, dataVersion, context);
    }
}
