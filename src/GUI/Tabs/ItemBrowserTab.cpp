#include "GUI/Tabs/ItemBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

#include <algorithm>

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

        struct DerivedItemCategoryCache
        {
            const std::vector<FormEntry>* source{ nullptr };
            std::size_t sourceSize{ 0 };
            std::vector<FormEntry> keys{};
            std::vector<FormEntry> notesBooks{};
            std::vector<FormEntry> aid{};
            std::vector<FormEntry> components{};
        };

        bool PassesLocalRecordFilters(const FormEntry& entry, const ItemBrowserTabContext& context)
        {
            if (!context.showPlayableRecords && entry.isPlayable) {
                return false;
            }
            if (!context.showNonPlayableRecords && !entry.isPlayable) {
                return false;
            }

            const bool hasName = !entry.name.empty();
            if (!context.showNamedRecords && hasName) {
                return false;
            }
            if (!context.showUnnamedRecords && !hasName) {
                return false;
            }

            if (!context.showDeletedRecords && entry.isDeleted) {
                return false;
            }

            return true;
        }

        const std::vector<FormEntry>& GetFilteredEntries(
            LocalFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            const ItemBrowserTabContext& context)
        {
            const bool needsRebuild =
                cacheState.source != &source ||
                cacheState.sourceSize != source.size() ||
                cacheState.showPlayable != context.showPlayableRecords ||
                cacheState.showNonPlayable != context.showNonPlayableRecords ||
                cacheState.showNamed != context.showNamedRecords ||
                cacheState.showUnnamed != context.showUnnamedRecords ||
                cacheState.showDeleted != context.showDeletedRecords;

            if (needsRebuild) {
                cacheState.filtered.clear();
                cacheState.filtered.reserve(source.size());

                for (const auto& entry : source) {
                    if (PassesLocalRecordFilters(entry, context)) {
                        cacheState.filtered.push_back(entry);
                    }
                }

                cacheState.source = &source;
                cacheState.sourceSize = source.size();
                cacheState.showPlayable = context.showPlayableRecords;
                cacheState.showNonPlayable = context.showNonPlayableRecords;
                cacheState.showNamed = context.showNamedRecords;
                cacheState.showUnnamed = context.showUnnamedRecords;
                cacheState.showDeleted = context.showDeletedRecords;
            }

            return cacheState.filtered;
        }

        const DerivedItemCategoryCache& GetDerivedCategoryCache(const FormCache& cache)
        {
            static DerivedItemCategoryCache derivedCache{};

            if (derivedCache.source == &cache.allRecords && derivedCache.sourceSize == cache.allRecords.size()) {
                return derivedCache;
            }

            derivedCache.source = &cache.allRecords;
            derivedCache.sourceSize = cache.allRecords.size();
            derivedCache.keys.clear();
            derivedCache.notesBooks.clear();
            derivedCache.aid.clear();
            derivedCache.components.clear();

            derivedCache.keys.reserve(cache.allRecords.size() / 32);
            derivedCache.notesBooks.reserve(cache.allRecords.size() / 24);
            derivedCache.aid.reserve(cache.allRecords.size() / 24);
            derivedCache.components.reserve(cache.allRecords.size() / 24);

            for (const auto& entry : cache.allRecords) {
                if (entry.category == "KEYM") {
                    derivedCache.keys.push_back(entry);
                    continue;
                }
                if (entry.category == "NOTE" || entry.category == "BOOK") {
                    derivedCache.notesBooks.push_back(entry);
                    continue;
                }
                if (entry.category == "ALCH") {
                    derivedCache.aid.push_back(entry);
                    continue;
                }
                if (entry.category == "CMPO") {
                    derivedCache.components.push_back(entry);
                }
            }

            return derivedCache;
        }

        void DrawItemTable(std::string_view tableId, const std::vector<FormEntry>& sourceItems, LocalFilterCache& filterCache, ItemBrowserTabContext& context)
        {
            const auto& items = GetFilteredEntries(filterCache, sourceItems, context);
            const FormTableConfig tableConfig{
                .tableId = tableId.data(),
                .primaryActionLabel = context.localize("Items", "sGiveItem", "Give Item"),
                .quantityActionLabel = context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"),
                .allowFavorites = true
            };

            FormTable::Draw(
                items,
                context.itemSearch,
                context.selectedPluginFilter,
                tableConfig,
                [&](const FormEntry& entry) {
                    context.openItemGrantPopup(entry);
                },
                [](const FormEntry& entry, int quantity) {
                    FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                },
                &context.favoriteForms,
                &context.searchCaseSensitive);
        }
    }

    void ItemBrowserTab::Draw(const FormCache& cache, ItemBrowserTabContext& context)
    {
        if (RecordFiltersWidget::Draw(
                context.localize,
                "ItemBrowser",
                RecordFilterState{
                    .showPlayable = context.showPlayableRecords,
                    .showNonPlayable = context.showNonPlayableRecords,
                    .showNamed = context.showNamedRecords,
                    .showUnnamed = context.showUnnamedRecords,
                    .showDeleted = context.showDeletedRecords })) {
            context.persistListFilters();
        }

        if (ImGui::BeginTable("ItemSearchRow", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Search", ImGuiTableColumnFlags_WidthStretch, 0.78f);
            ImGui::TableSetupColumn("Options", ImGuiTableColumnFlags_WidthStretch, 0.22f);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            SearchBar::Draw(context.localize("Items", "sSearch", "Item Search"), context.itemSearchBuffer, context.itemSearchBufferSize, context.itemSearch);
            ImGui::TableNextColumn();
            if (ImGui::Checkbox(context.localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &context.searchCaseSensitive)) {
                context.persistFilterCheckboxes();
            }
            ImGui::EndTable();
        }

        context.drawPluginFilterStatus();
        ImGui::Separator();

        static LocalFilterCache weaponFilterCache{};
        static LocalFilterCache armorFilterCache{};
        static LocalFilterCache ammoFilterCache{};
        static LocalFilterCache miscFilterCache{};
        static LocalFilterCache keysFilterCache{};
        static LocalFilterCache notesBooksFilterCache{};
        static LocalFilterCache aidFilterCache{};
        static LocalFilterCache componentsFilterCache{};

        const auto& derivedCache = GetDerivedCategoryCache(cache);

        if (ImGui::BeginTabBar("ItemCategories")) {
            if (ImGui::BeginTabItem(context.localize("Items", "sWeapons", "Weapons"))) {
                DrawItemTable("ItemTableWeapons", cache.weapons, weaponFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("Items", "sArmor", "Armor"))) {
                DrawItemTable("ItemTableArmor", cache.armors, armorFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("Items", "sAmmo", "Ammo"))) {
                DrawItemTable("ItemTableAmmo", cache.ammo, ammoFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("Items", "sMisc", "Misc"))) {
                DrawItemTable("ItemTableMisc", cache.misc, miscFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("General", "sKeys", "Keys"))) {
                DrawItemTable("ItemTableKeys", derivedCache.keys, keysFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("General", "sHolotapesNotes", "Holotapes/Books"))) {
                DrawItemTable("ItemTableNotesBooks", derivedCache.notesBooks, notesBooksFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("General", "sAidChems", "Aid"))) {
                DrawItemTable("ItemTableAid", derivedCache.aid, aidFilterCache, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("General", "sComponents", "Components"))) {
                DrawItemTable("ItemTableComponents", derivedCache.components, componentsFilterCache, context);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
