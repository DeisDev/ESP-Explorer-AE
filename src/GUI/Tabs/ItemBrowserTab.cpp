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

        void DrawItemTable(std::string_view tableId, std::vector<FormEntry> items, ItemBrowserTabContext& context)
        {
            items.erase(std::remove_if(items.begin(), items.end(), [&](const FormEntry& entry) {
                return !PassesLocalRecordFilters(entry, context);
            }), items.end());
            const FormTableConfig tableConfig{
                .tableId = tableId.data(),
                .primaryActionLabel = context.localize("Items", "sGiveItem", "Give Item"),
                .quantityActionLabel = context.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"),
                .allowFavorites = true
            };

            FormTable::Draw(
                std::move(items),
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
            ImGui::Checkbox(context.localize("General", "sCaseSensitiveSearch", "Case Sensitive"), &context.searchCaseSensitive);
            ImGui::EndTable();
        }

        context.drawPluginFilterStatus();
        ImGui::Separator();

        if (ImGui::BeginTabBar("ItemCategories")) {
            if (ImGui::BeginTabItem(context.localize("Items", "sWeapons", "Weapons"))) {
                DrawItemTable("ItemTableWeapons", cache.weapons, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("Items", "sArmor", "Armor"))) {
                DrawItemTable("ItemTableArmor", cache.armors, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("Items", "sAmmo", "Ammo"))) {
                DrawItemTable("ItemTableAmmo", cache.ammo, context);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(context.localize("Items", "sMisc", "Misc"))) {
                DrawItemTable("ItemTableMisc", cache.misc, context);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}
