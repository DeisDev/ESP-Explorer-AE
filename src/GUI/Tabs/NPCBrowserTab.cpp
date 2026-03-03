#include "GUI/Tabs/NPCBrowserTab.h"

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
            const NPCBrowserTab::FilterEntriesFn& filterEntries)
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
        const std::function<void()>& persistFilterCheckboxes,
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
            if (ImGui::Checkbox(localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &searchCaseSensitive)) {
                persistFilterCheckboxes();
            }
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

        static LocalFilterCache npcFilterCache{};
        const auto& filteredNPCs = GetFilteredEntries(
            npcFilterCache,
            cache.npcs,
            showPlayableRecords,
            showNonPlayableRecords,
            showNamedRecords,
            showUnnamedRecords,
            showDeletedRecords,
            filterEntries);

        std::vector<std::string> categories;
        categories.reserve(filteredNPCs.size());
        std::unordered_set<std::string> seenCategories;
        seenCategories.reserve(filteredNPCs.size());

        for (const auto& entry : filteredNPCs) {
            if (seenCategories.insert(entry.category).second) {
                categories.push_back(entry.category);
            }
        }

        std::ranges::sort(categories, [](const std::string& left, const std::string& right) {
            if (left.empty()) {
                return false;
            }
            if (right.empty()) {
                return true;
            }
            return left < right;
        });

        if (ImGui::BeginTabBar("NPCCategories")) {
            for (const auto& category : categories) {
                const char* displayCategory = category.empty() ? localize("General", "sUnknown", "Unknown") : category.c_str();
                std::size_t categoryCount = 0;
                for (const auto& entry : filteredNPCs) {
                    if (entry.category == category) {
                        ++categoryCount;
                    }
                }

                const std::string tabLabel = std::string(displayCategory) + " (" + std::to_string(categoryCount) + ")";
                if (!ImGui::BeginTabItem(tabLabel.c_str())) {
                    continue;
                }

                std::vector<FormEntry> categoryEntries;
                categoryEntries.reserve(categoryCount);
                for (const auto& entry : filteredNPCs) {
                    if (entry.category == category) {
                        categoryEntries.push_back(entry);
                    }
                }

                const std::string categoryTableId = std::string("NPCTable_") + (category.empty() ? "Unknown" : category);
                const FormTableConfig categoryTableConfig{
                    .tableId = categoryTableId.c_str(),
                    .primaryActionLabel = tableConfig.primaryActionLabel,
                    .quantityActionLabel = tableConfig.quantityActionLabel,
                    .allowFavorites = tableConfig.allowFavorites
                };

                FormTable::Draw(
                    categoryEntries,
                    searchText,
                    selectedPluginFilter,
                    categoryTableConfig,
                    [](const FormEntry& entry) {
                        FormActions::SpawnAtPlayer(entry.formID, 1);
                    },
                    [](const FormEntry& entry, int quantity) {
                        FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                    },
                    &favoriteForms,
                    &searchCaseSensitive);

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
