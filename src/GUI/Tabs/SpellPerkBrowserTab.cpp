#include "GUI/Tabs/SpellPerkBrowserTab.h"
#include "GUI/Widgets/BrowserWidgets.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void SpellPerkBrowserTab::Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests)
    {
        BrowserWidgets::DrawControls(state, view, requests, "SpellPerkBrowser", "Spells", "Spell/Perk Search");
        struct Category { const char* type; const char* table; const char* key; const char* fallback; ActionKind add; ActionKind remove; };
        static constexpr Category categories[]{
            { "SPEL", "SpellTable", "sSpells", "Spells", ActionKind::AddSpell, ActionKind::RemoveSpell },
            { "PERK", "PerkTable", "sPerks", "Perks", ActionKind::AddPerk, ActionKind::RemovePerk }
        };
        if (!ImGui::BeginTabBar("SpellPerkCategories")) return;
        for (const auto& category : categories) {
            const auto label = std::string(view.localize("Spells", category.key, category.fallback)) + "###" + category.type;
            const bool open = ImGui::BeginTabItem(label.c_str(), nullptr, state.requestedCategory == category.type ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);
            SharedUtils::DrawCurrentItemChrome(open, ImGui::IsItemHovered(), true, false);
            if (!open) continue;
            if (state.requestedCategory == category.type) state.requestedCategory.clear();
            BrowserWidgets::ActivateCategory(state, view, category.type);
            const bool spell = category.add == ActionKind::AddSpell;
            const FormTableConfig config{
                .tableId = category.table,
                .primaryActionLabel = spell ? view.localize("Spells", "sAddSpell", "Add Spell") : view.localize("Spells", "sAddPerk", "Add Perk"),
                .secondaryActionLabel = spell ? view.localize("General", "sRemoveSpellEffect", "Remove Spell/Effect") : view.localize("General", "sRemovePerk", "Remove Perk"),
                .allowFavorites = true, .gameplayActionsAllowed = view.gameplayReady
            };
            BrowserWidgets::DrawCategory(state, view, requests, category.type, config, category.add, category.remove);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
