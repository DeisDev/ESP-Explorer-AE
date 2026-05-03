#include "Config/Config.h"
#include "GUI/Tabs/SpellPerkBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFilterCache.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        const std::vector<FormEntry>& GetFilteredEntries(
            RecordFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            std::uint64_t advancedFilterRevision,
            const SpellPerkBrowserTab::FilterEntriesFn& filterEntries)
        {
            return RecordFilterCache::GetFiltered(cacheState, source, showPlayable, showNonPlayable, showNamed, showUnnamed, showDeleted, advancedFilterRevision, filterEntries);
        }
    }

    void SpellPerkBrowserTab::Draw(
        const FormCache& cache,
        char* searchBuffer,
        std::size_t searchBufferSize,
        std::string& searchText,
        std::string_view selectedPluginFilter,
        bool& showPlayableRecords,
        bool& showNonPlayableRecords,
        bool& showNamedRecords,
        bool& showUnnamedRecords,
        bool& showDeletedRecords,
        std::vector<AdvancedFilterRule>& advancedRules,
        std::unordered_set<std::string>& hiddenPlugins,
        std::uint64_t advancedFilterRevision,
        bool* searchFocusPending,
        std::unordered_set<std::uint32_t>& favoriteForms,
        const std::function<void()>& drawPluginFilterStatus,
        const std::function<void()>& persistListFilters,
        const std::function<void()>&,
        const FilterEntriesFn& filterEntries,
        const LocalizeFn& localize,
        const ContextMenuCallbacks* contextCallbacks)
    {
        if (RecordFiltersWidget::Draw(
                localize,
                "SpellPerkBrowser",
                RecordFilterState{
                    .showNonPlayable = showNonPlayableRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords,
                    .advancedRules = advancedRules,
                    .hiddenPlugins = hiddenPlugins })) {
            persistListFilters();
        }

        SearchBar::Draw(localize("Spells", "sSearch", "Spell/Perk Search"), searchBuffer, searchBufferSize, searchText, searchFocusPending);
        drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("SpellPerkCategories")) {
            static int lastActiveCategory = -1;
            int activeCategory = -1;
            static RecordFilterCache spellFilterCache{};
            static RecordFilterCache perkFilterCache{};

            const auto& filteredSpells = GetFilteredEntries(
                spellFilterCache,
                cache.spells,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedFilterRevision,
                filterEntries);
            const auto& filteredPerks = GetFilteredEntries(
                perkFilterCache,
                cache.perks,
                showPlayableRecords,
                showNonPlayableRecords,
                showNamedRecords,
                showUnnamedRecords,
                showDeletedRecords,
                advancedFilterRevision,
                filterEntries);

            const FormTableConfig spellConfig{
                .tableId = "SpellTable",
                .primaryActionLabel = localize("Spells", "sAddSpell", "Add Spell"),
                .secondaryActionLabel = localize("General", "sRemoveSpellEffect", "Remove Spell/Effect"),
                .allowFavorites = true
            };
            const FormTableConfig perkConfig{
                .tableId = "PerkTable",
                .primaryActionLabel = localize("Spells", "sAddPerk", "Add Perk"),
                .secondaryActionLabel = localize("General", "sRemovePerk", "Remove Perk"),
                .allowFavorites = true
            };

            const bool spellsTabOpen = ImGui::BeginTabItem(localize("Spells", "sSpells", "Spells"));
            SharedUtils::DrawCurrentItemChrome(spellsTabOpen, ImGui::IsItemHovered(), true, false);
            if (spellsTabOpen) {
                activeCategory = 0;
                FormTable::Draw(
                    filteredSpells,
                    searchText,
                    selectedPluginFilter,
                    spellConfig,
                    [](const FormEntry& entry) {
                        FormActions::AddSpellToPlayer(entry.formID);
                    },
                    {},
                    {},
                    &favoriteForms,
                    contextCallbacks,
                    [](const std::vector<FormEntry>& entries) {
                        for (const auto& entry : entries) {
                            FormActions::RemoveSpellFromPlayer(entry.formID);
                        }
                    });
                ImGui::EndTabItem();
            }

            const bool perksTabOpen = ImGui::BeginTabItem(localize("Spells", "sPerks", "Perks"));
            SharedUtils::DrawCurrentItemChrome(perksTabOpen, ImGui::IsItemHovered(), true, false);
            if (perksTabOpen) {
                activeCategory = 1;
                FormTable::Draw(
                    filteredPerks,
                    searchText,
                    selectedPluginFilter,
                    perkConfig,
                    [](const FormEntry& entry) {
                        FormActions::AddPerkToPlayer(entry.formID);
                    },
                    {},
                    {},
                    &favoriteForms,
                    contextCallbacks,
                    [](const std::vector<FormEntry>& entries) {
                        for (const auto& entry : entries) {
                            FormActions::RemovePerkFromPlayer(entry.formID);
                        }
                    });
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();

            if (activeCategory != -1) {
                if (searchFocusPending && lastActiveCategory != -1 && activeCategory != lastActiveCategory && Config::Get().autoFocusSearchBars) {
                    *searchFocusPending = true;
                }
                lastActiveCategory = activeCategory;
            }
        }
    }
}
