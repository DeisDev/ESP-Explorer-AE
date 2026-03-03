#include "GUI/Tabs/SpellPerkBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        struct LocalFilterCache
        {
            const std::vector<FormEntry>* source{ nullptr };
            std::size_t sourceSize{ 0 };
            bool showPlayable{ true };
            bool showNonPlayable{ true };
            bool showNamed{ true };
            bool showUnnamed{ true };
            bool showDeleted{ true };
            std::vector<FormEntry> filtered{};
        };

        const std::vector<FormEntry>& GetFilteredEntries(
            LocalFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            const SpellPerkBrowserTab::FilterEntriesFn& filterEntries)
        {
            const bool needsRebuild =
                cacheState.source != &source ||
                cacheState.sourceSize != source.size() ||
                cacheState.showPlayable != showPlayable ||
                cacheState.showNonPlayable != showNonPlayable ||
                cacheState.showNamed != showNamed ||
                cacheState.showUnnamed != showUnnamed ||
                cacheState.showDeleted != showDeleted;

            if (needsRebuild) {
                cacheState.filtered = filterEntries(source);
                cacheState.source = &source;
                cacheState.sourceSize = source.size();
                cacheState.showPlayable = showPlayable;
                cacheState.showNonPlayable = showNonPlayable;
                cacheState.showNamed = showNamed;
                cacheState.showUnnamed = showUnnamed;
                cacheState.showDeleted = showDeleted;
            }

            return cacheState.filtered;
        }
    }

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
        const std::function<void()>& persistFilterCheckboxes,
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
            if (ImGui::Checkbox(localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &searchCaseSensitive)) {
                persistFilterCheckboxes();
            }
            ImGui::EndTable();
        }
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("SpellPerkCategories")) {
            static LocalFilterCache spellFilterCache{};
            static LocalFilterCache perkFilterCache{};

            const auto& filteredSpells = GetFilteredEntries(
                spellFilterCache,
                cache.spells,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                filterEntries);
            const auto& filteredPerks = GetFilteredEntries(
                perkFilterCache,
                cache.perks,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                filterEntries);

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
                    filteredSpells,
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
                    filteredPerks,
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
