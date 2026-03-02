#include "GUI/Tabs/SpellPerkBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void SpellPerkBrowserTab::Draw(
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
                "SpellPerkBrowser",
                RecordFilterState{
                    .showPlayable = showPlayableRecords,
                    .showNonPlayable = showNonPlayableRecords,
                    .showNamed = showNamedRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords })) {
            persistListFilters();
        }

        if (ImGui::BeginTable("SpellPerkSearchRow", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 0.78f);
            ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 0.22f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            SearchBar::Draw(localize("Spells", "sSearch", "Spell/Perk Search"), searchBuffer, searchBufferSize, searchText);
            ImGui::TableNextColumn();
            ImGui::Checkbox(localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &searchCaseSensitive);
            ImGui::EndTable();
        }
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("SpellPerkCategories")) {
            const FormTableConfig spellConfig{
                .tableId = "SpellTable",
                .primaryActionLabel = localize("Spells", "sAddSpell", "Add Spell"),
                .allowFavorites = true
            };
            const FormTableConfig perkConfig{
                .tableId = "PerkTable",
                .primaryActionLabel = localize("Spells", "sAddPerk", "Add Perk"),
                .allowFavorites = true
            };

            if (ImGui::BeginTabItem(localize("Spells", "sSpells", "Spells"))) {
                FormTable::Draw(
                    filterEntries(cache.spells),
                    searchText,
                    selectedPluginFilter,
                    spellConfig,
                    [](const FormEntry& entry) {
                        FormActions::AddSpellToPlayer(entry.formID);
                    },
                    {},
                    &favoriteForms,
                    &searchCaseSensitive);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(localize("Spells", "sPerks", "Perks"))) {
                FormTable::Draw(
                    filterEntries(cache.perks),
                    searchText,
                    selectedPluginFilter,
                    perkConfig,
                    [](const FormEntry& entry) {
                        FormActions::AddPerkToPlayer(entry.formID);
                    },
                    {},
                    &favoriteForms,
                    &searchCaseSensitive);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
