#include "GUI/Widgets/BrowserWidgets.h"

#include "Core/RecordActions.h"
#include "GUI/Widgets/FormatUtils.h"

#include <imgui.h>

namespace ESPExplorerAE::BrowserWidgets
{
    void Emit(BrowserRequests& requests, const BrowserView& view, const FormEntry& entry, ActionKind kind, bool confirm, std::uint32_t count)
    {
        if (auto request = PrepareRecordAction(kind, entry, view.session, view.gameplayReady, count)) {
            if (kind == ActionKind::Equip) request->ammoCount = view.equipWeaponAmmoCount;
            requests.actions.push_back({ std::move(*request), confirm, entry.name });
        }
    }

    void DrawContext(const FormEntry& entry, ContextScope scope, std::unordered_map<std::uint32_t, int>& quantities, const BrowserView& view, BrowserRequests& requests)
    {
        const auto& localize = view.localize;
        const auto* name = entry.name.empty() ? localize("General", "sUnnamed", "<Unnamed>") : entry.name.c_str();
        const auto id = FormatUtils::FormID(entry.formID);
        ImGui::TextUnformatted(name);
        ImGui::TextDisabled("%s  |  %s  |  %s", id.c_str(), entry.sourcePlugin.c_str(), entry.category.c_str());
        if (!entry.editorID.empty()) ImGui::TextDisabled("%s: %s", localize("General", "sEditorID", "EditorID"), entry.editorID.c_str());
        ImGui::Separator();
        ImGui::BeginDisabled(!view.gameplayReady || scope == ContextScope::Selection || entry.isDeleted);
        if (SupportsRecordAction(entry.category, ActionKind::Give) && ImGui::MenuItem(localize("Items", "sGiveItem", "Give Item"))) {
            requests.grants.push_back({ view.session, { entry.formID } });
        }
        if (SupportsRecordAction(entry.category, ActionKind::Spawn)) {
            auto& quantity = quantities.try_emplace(entry.formID, 1).first->second;
            if (ImGui::MenuItem(localize("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) Emit(requests, view, entry, ActionKind::Spawn, !SupportsRecordAction(entry.category, ActionKind::Give), quantity);
            const auto normal = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
            const auto hovered = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
            const auto active = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(normal.x, normal.y, normal.z, (std::max)(normal.w, 0.30f)));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(hovered.x, hovered.y, hovered.z, (std::max)(hovered.w, 0.36f)));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(active.x, active.y, active.z, (std::max)(active.w, 0.42f)));
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt((std::string(localize("General", "sQuantity", "Quantity")) + "###SpawnQuantity").c_str(), &quantity, 1, 10);
            ImGui::PopStyleColor(3);
            quantity = (std::clamp)(quantity, 1, static_cast<int>(ActionQueue::MaxQuantity));
        }
        if (SupportsRecordAction(entry.category, ActionKind::Equip) && ImGui::MenuItem(localize("General", "sEquipItem", "Equip Item"))) Emit(requests, view, entry, ActionKind::Equip);
        const auto actionItem = [&](ActionKind kind, const char* key, const char* fallback) {
            if (SupportsRecordAction(entry.category, kind) && ImGui::MenuItem(localize("General", key, fallback))) Emit(requests, view, entry, kind, true);
        };
        actionItem(ActionKind::AddSpell, "sAddSpellEffect", "Add Spell/Effect");
        actionItem(ActionKind::RemoveSpell, "sRemoveSpellEffect", "Remove Spell/Effect");
        actionItem(ActionKind::AddPerk, "sAddPerk", "Add Perk");
        actionItem(ActionKind::RemovePerk, "sRemovePerk", "Remove Perk");
        actionItem(ActionKind::StartQuest, "sStartQuest", "Start Quest");
        actionItem(ActionKind::CompleteQuest, "sCompleteQuest", "Complete Quest");
        actionItem(ActionKind::SetWeather, "sSetWeather", "Set Weather");
        if (SupportsRecordAction(entry.category, ActionKind::PlaySound) && !entry.editorID.empty() &&
            ImGui::MenuItem(localize("General", "sPlaySound", "Play Sound"))) Emit(requests, view, entry, ActionKind::PlaySound);
        if (SupportsRecordAction(entry.category, ActionKind::SetGlobal) &&
            ImGui::MenuItem(localize("General", "sSetGlobal", "Set Global"), nullptr, false, !entry.editorID.empty())) requests.globalValues.push_back({view.session, entry.formID});
        actionItem(ActionKind::Outfit, "sAddOutfitItems", "Add Outfit Items");
        actionItem(ActionKind::ConstructedItem, "sAddCraftedItem", "Add Crafted Item");
        if (SupportsRecordAction(entry.category, ActionKind::Teleport)) {
            ImGui::BeginDisabled(!CanTeleportRecord(entry));
            if (ImGui::MenuItem(localize("General", "sTeleportCOC", "Teleport (COC)"))) Emit(requests, view, entry, ActionKind::Teleport, true);
            ImGui::EndDisabled();
        }
        ImGui::EndDisabled();
        if (scope != ContextScope::Single) return;
        ImGui::Separator();
        if (ImGui::MenuItem(localize("General", "sCopyFormID", "Copy FormID"))) ImGui::SetClipboardText(id.c_str());
        if (ImGui::MenuItem(localize("General", "sCopyRecordSource", "Copy Record Source"))) ImGui::SetClipboardText(entry.sourcePlugin.c_str());
        if (ImGui::MenuItem(localize("General", "sCopyName", "Copy Name"))) ImGui::SetClipboardText(name);
        if (!entry.editorID.empty() && ImGui::MenuItem(localize("General", "sCopyEditorID", "Copy EditorID"))) ImGui::SetClipboardText(entry.editorID.c_str());
        ImGui::Separator();
        const bool favorite = view.favorites.contains(entry.formID);
        if (ImGui::MenuItem(favorite ? localize("General", "sRemoveFavorite", "Remove Favorite") : localize("General", "sAddFavorite", "Add Favorite"))) {
            if (favorite) view.favorites.erase(entry.formID);
            else view.favorites.insert(entry.formID);
        }
    }

}
