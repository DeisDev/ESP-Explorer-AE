#include "GUI/Tabs/NPCBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void NPCBrowserTab::Draw(
        const FormCache& cache,
        char* searchBuffer,
        std::size_t searchBufferSize,
        std::string& searchText,
        bool& searchCaseSensitive,
        std::string_view selectedPluginFilter,
        bool& showPlayableRecords,
        bool& showNonPlayableRecords,
        bool& showNamedRecords,
        bool& showUnnamedRecords,
        bool& showDeletedRecords,
        std::unordered_set<std::uint32_t>& favoriteForms,
        const std::function<void()>& drawPluginFilterStatus,
        const std::function<void()>& persistListFilters,
        const FilterEntriesFn& filterEntries,
        const LocalizeFn& localize)
    {
        if (RecordFiltersWidget::Draw(
                localize,
                "NPCBrowser",
                RecordFilterState{
                    .showPlayable = showPlayableRecords,
                    .showNonPlayable = showNonPlayableRecords,
                    .showNamed = showNamedRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords })) {
            persistListFilters();
        }

        if (ImGui::BeginTable("NPCSearchRow", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 0.78f);
            ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 0.22f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            SearchBar::Draw(localize("NPCs", "sSearch", "NPC Search"), searchBuffer, searchBufferSize, searchText);
            ImGui::TableNextColumn();
            ImGui::Checkbox(localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &searchCaseSensitive);
            ImGui::EndTable();
        }
        drawPluginFilterStatus();
        ImGui::Separator();

        const FormTableConfig tableConfig{
            .tableId = "NPCTable",
            .primaryActionLabel = localize("NPCs", "sSpawnNPC", "Spawn"),
            .quantityActionLabel = localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"),
            .allowFavorites = true
        };

        FormTable::Draw(
            filterEntries(cache.npcs),
            searchText,
            selectedPluginFilter,
            tableConfig,
            [](const FormEntry& entry) {
                FormActions::SpawnAtPlayer(entry.formID, 1);
            },
            [](const FormEntry& entry, int quantity) {
                FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
            },
            &favoriteForms,
            &searchCaseSensitive);
    }
}
