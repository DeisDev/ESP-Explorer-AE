#include "GUI/Tabs/ItemBrowserTab.h"
#include "Core/RecordActions.h"
#include "GUI/Widgets/BrowserWidgets.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void ItemBrowserTab::Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests)
    {
        BrowserWidgets::DrawControls(state, view, requests, "ItemBrowser", "Items", "Item Search");
        struct Category { const char* id; const char* section; const char* key; const char* fallback; std::vector<std::string> types; };
        static const Category categories[]{
            { "ItemTableAll", "General", "sAll", "All", { "WEAP", "ARMO", "AMMO", "MISC", "KEYM", "NOTE", "BOOK", "ALCH", "CMPO" } },
            { "ItemTableWeapons", "Items", "sWeapons", "Weapons", { "WEAP" } },
            { "ItemTableArmor", "Items", "sArmor", "Armor", { "ARMO" } },
            { "ItemTableAmmo", "Items", "sAmmo", "Ammo", { "AMMO" } },
            { "ItemTableMisc", "Items", "sMisc", "Misc", { "MISC" } },
            { "ItemTableKeys", "General", "sKeys", "Keys", { "KEYM" } },
            { "ItemTableNotesBooks", "General", "sHolotapesNotes", "Holotapes/Books", { "NOTE", "BOOK" } },
            { "ItemTableAid", "General", "sAidChems", "Aid", { "ALCH" } },
            { "ItemTableComponents", "General", "sComponents", "Components", { "CMPO" } }
        };
        if (!ImGui::BeginTabBar("ItemCategories")) return;
        for (const auto& category : categories) {
            const auto label = std::string(view.localize(category.section, category.key, category.fallback)) + "###" + category.id;
            const bool open = ImGui::BeginTabItem(label.c_str(), nullptr, state.requestedCategory == category.id ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);
            SharedUtils::DrawCurrentItemChrome(open, ImGui::IsItemHovered(), true, false);
            if (!open) continue;
            if (state.requestedCategory == category.id) state.requestedCategory.clear();
            BrowserWidgets::ActivateCategory(state, view, category.id);
            auto& rows = state.categories[category.id];
            auto query = BrowserWidgets::MakeQuery(state, view, rows, {});
            query.types = category.types;
            const auto& result = rows.query.Update(view.catalog, query, view.filters.advancedRecordFilters, view.filters.advancedRecordFilterRevision);
            const FormTableConfig config{
                .tableId = category.id, .primaryActionLabel = view.localize("Items", "sGiveItem", "Give Item"),
                .quantityActionLabel = view.localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"),
                .allowFavorites = true, .gameplayActionsAllowed = view.gameplayReady, .copyFormat = view.copyFormat, .doubleClickGameplayAction = view.doubleClickGameplayAction, .compactDensity = view.compactTableDensity
            };
            const FormTableActions actions{
                .primary = [&](const FormEntry& entry) { requests.grants.push_back({ view.session, { entry.formID } }); },
                .bulkPrimary = [&](const std::vector<FormEntry>& entries) {
                    BrowserRequests::Grant grant{ view.session, {} };
                    for (const auto& entry : entries) if (!entry.isDeleted) grant.forms.push_back(entry.formID);
                    if (!grant.forms.empty()) requests.grants.push_back(std::move(grant));
                },
                .quantity = [&](const FormEntry& entry, int quantity) { BrowserWidgets::Emit(requests, view, entry, ActionKind::Spawn, false, quantity); },
                .rowContext = [&](const FormEntry& entry, bool multiple) { BrowserWidgets::DrawContext(entry, multiple ? BrowserWidgets::ContextScope::Selection : BrowserWidgets::ContextScope::Single, rows.contextQuantities, view, requests); },
                .canPrimary = [](const FormEntry& entry) { return !entry.isDeleted && SupportsRecordAction(entry.category, ActionKind::Give); },
            .selected = [&](auto id) { requests.recentSelections.push_back(id); },
            .inspect = [&](auto id) { requests.inspections.push_back(id); },
            .basket = [&](const auto& entries) { for (const auto& entry : entries) requests.basket.push_back(entry.formID); },
            .collect = [&](const auto& entries) { for (const auto& entry : entries) requests.collections.push_back(entry.formID); }
            };
            FormTable::DrawPrepared(rows.table, result, config, actions, &view.favorites);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
