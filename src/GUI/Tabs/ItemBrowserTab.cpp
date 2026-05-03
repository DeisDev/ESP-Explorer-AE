#include "Config/Config.h"
#include "GUI/Tabs/ItemBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SharedUtils.h"

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
            std::uint64_t advancedFilterRevision{ 0 };
            std::vector<FormEntry> filtered{};
        };

        struct DerivedItemCategoryCache
        {
            const std::vector<FormEntry>* source{ nullptr };
            std::size_t sourceSize{ 0 };
            std::size_t weaponSize{ 0 };
            std::size_t armorSize{ 0 };
            std::size_t ammoSize{ 0 };
            std::size_t miscSize{ 0 };
            std::vector<FormEntry> allItems{};
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

            if (context.passesAdvancedFilters && !context.passesAdvancedFilters(entry)) {
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
                cacheState.showDeleted != context.showDeletedRecords ||
                cacheState.advancedFilterRevision != context.advancedFilterRevision;

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
                cacheState.advancedFilterRevision = context.advancedFilterRevision;
            }

            return cacheState.filtered;
        }

        const DerivedItemCategoryCache& GetDerivedCategoryCache(const FormCache& cache)
        {
            static DerivedItemCategoryCache derivedCache{};

            if (derivedCache.source == &cache.allRecords && derivedCache.sourceSize == cache.allRecords.size() &&
                derivedCache.weaponSize == cache.weapons.size() && derivedCache.armorSize == cache.armors.size() &&
                derivedCache.ammoSize == cache.ammo.size() && derivedCache.miscSize == cache.misc.size()) {
                return derivedCache;
            }

            derivedCache.source = &cache.allRecords;
            derivedCache.sourceSize = cache.allRecords.size();
            derivedCache.weaponSize = cache.weapons.size();
            derivedCache.armorSize = cache.armors.size();
            derivedCache.ammoSize = cache.ammo.size();
            derivedCache.miscSize = cache.misc.size();
            derivedCache.allItems.clear();
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

            const auto totalItems = cache.weapons.size() + cache.armors.size() + cache.ammo.size() + cache.misc.size() +
                derivedCache.keys.size() + derivedCache.notesBooks.size() + derivedCache.aid.size() + derivedCache.components.size();
            derivedCache.allItems.clear();
            derivedCache.allItems.reserve(totalItems);
            derivedCache.allItems.insert(derivedCache.allItems.end(), cache.weapons.begin(), cache.weapons.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), cache.armors.begin(), cache.armors.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), cache.ammo.begin(), cache.ammo.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), cache.misc.begin(), cache.misc.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), derivedCache.keys.begin(), derivedCache.keys.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), derivedCache.notesBooks.begin(), derivedCache.notesBooks.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), derivedCache.aid.begin(), derivedCache.aid.end());
            derivedCache.allItems.insert(derivedCache.allItems.end(), derivedCache.components.begin(), derivedCache.components.end());

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
                [&](const std::vector<FormEntry>& selectedEntries) {
                    context.openItemGrantPopupMultiple(selectedEntries);
                },
                [](const FormEntry& entry, int quantity) {
                    FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
                },
                &context.favoriteForms,
                &context.contextCallbacks);
        }
    }

    void ItemBrowserTab::Draw(const FormCache& cache, ItemBrowserTabContext& context)
    {
        if (RecordFiltersWidget::Draw(
                context.localize,
                "ItemBrowser",
                RecordFilterState{
                    .showNonPlayable = context.showNonPlayableRecords,
                    .showUnnamed = context.showUnnamedRecords,
                    .showDeleted = context.showDeletedRecords,
                    .advancedRules = context.advancedRules,
                    .hiddenPlugins = context.hiddenPlugins })) {
            context.persistListFilters();
        }

        SearchBar::Draw(context.localize("Items", "sSearch", "Item Search"), context.itemSearchBuffer, context.itemSearchBufferSize, context.itemSearch, context.searchFocusPending);

        context.drawPluginFilterStatus();
        ImGui::Separator();

        static LocalFilterCache allItemsFilterCache{};
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
            static int lastActiveCategory = -1;
            int activeCategory = -1;

            const bool allTabOpen = ImGui::BeginTabItem(context.localize("General", "sAll", "All"));
            SharedUtils::DrawCurrentItemChrome(allTabOpen, ImGui::IsItemHovered(), true, false);
            if (allTabOpen) {
                activeCategory = 0;
                DrawItemTable("ItemTableAll", derivedCache.allItems, allItemsFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool weaponsTabOpen = ImGui::BeginTabItem(context.localize("Items", "sWeapons", "Weapons"));
            SharedUtils::DrawCurrentItemChrome(weaponsTabOpen, ImGui::IsItemHovered(), true, false);
            if (weaponsTabOpen) {
                activeCategory = 1;
                DrawItemTable("ItemTableWeapons", cache.weapons, weaponFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool armorTabOpen = ImGui::BeginTabItem(context.localize("Items", "sArmor", "Armor"));
            SharedUtils::DrawCurrentItemChrome(armorTabOpen, ImGui::IsItemHovered(), true, false);
            if (armorTabOpen) {
                activeCategory = 2;
                DrawItemTable("ItemTableArmor", cache.armors, armorFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool ammoTabOpen = ImGui::BeginTabItem(context.localize("Items", "sAmmo", "Ammo"));
            SharedUtils::DrawCurrentItemChrome(ammoTabOpen, ImGui::IsItemHovered(), true, false);
            if (ammoTabOpen) {
                activeCategory = 3;
                DrawItemTable("ItemTableAmmo", cache.ammo, ammoFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool miscTabOpen = ImGui::BeginTabItem(context.localize("Items", "sMisc", "Misc"));
            SharedUtils::DrawCurrentItemChrome(miscTabOpen, ImGui::IsItemHovered(), true, false);
            if (miscTabOpen) {
                activeCategory = 4;
                DrawItemTable("ItemTableMisc", cache.misc, miscFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool keysTabOpen = ImGui::BeginTabItem(context.localize("General", "sKeys", "Keys"));
            SharedUtils::DrawCurrentItemChrome(keysTabOpen, ImGui::IsItemHovered(), true, false);
            if (keysTabOpen) {
                activeCategory = 5;
                DrawItemTable("ItemTableKeys", derivedCache.keys, keysFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool notesTabOpen = ImGui::BeginTabItem(context.localize("General", "sHolotapesNotes", "Holotapes/Books"));
            SharedUtils::DrawCurrentItemChrome(notesTabOpen, ImGui::IsItemHovered(), true, false);
            if (notesTabOpen) {
                activeCategory = 6;
                DrawItemTable("ItemTableNotesBooks", derivedCache.notesBooks, notesBooksFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool aidTabOpen = ImGui::BeginTabItem(context.localize("General", "sAidChems", "Aid"));
            SharedUtils::DrawCurrentItemChrome(aidTabOpen, ImGui::IsItemHovered(), true, false);
            if (aidTabOpen) {
                activeCategory = 7;
                DrawItemTable("ItemTableAid", derivedCache.aid, aidFilterCache, context);
                ImGui::EndTabItem();
            }
            const bool componentsTabOpen = ImGui::BeginTabItem(context.localize("General", "sComponents", "Components"));
            SharedUtils::DrawCurrentItemChrome(componentsTabOpen, ImGui::IsItemHovered(), true, false);
            if (componentsTabOpen) {
                activeCategory = 8;
                DrawItemTable("ItemTableComponents", derivedCache.components, componentsFilterCache, context);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();

            if (activeCategory != -1) {
                if (context.searchFocusPending && lastActiveCategory != -1 && activeCategory != lastActiveCategory && Config::Get().autoFocusSearchBars) {
                    *context.searchFocusPending = true;
                }
                lastActiveCategory = activeCategory;
            }
        }
    }
}
