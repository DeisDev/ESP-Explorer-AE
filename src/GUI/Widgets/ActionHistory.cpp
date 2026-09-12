#include "GUI/Widgets/ActionHistory.h"
#include "GUI/Widgets/InventoryFeedback.h"
#include "GUI/Widgets/FormatUtils.h"
#include <format>

namespace ESPExplorerAE
{
    std::vector<ActionHistoryEntry> FormatActionHistory(std::span<const ActionReceipt> receipts, const ActionHistoryLocalize& localize)
    {
        const auto L = [&](std::string_view section, std::string_view key, const char* fallback) {
            return std::string(localize ? localize(section, key, fallback) : fallback);
        };
        const auto statusLabel = [&](ActionStatus status) {
            switch (status) {
            case ActionStatus::Rejected: return L("General", "sActionStatusRejected", "Rejected");
            case ActionStatus::Dispatched: return L("General", "sActionStatusDispatched", "Dispatched");
            case ActionStatus::VerifiedChanged: return L("General", "sActionStatusChanged", "Change verified");
            case ActionStatus::NoChange: return L("General", "sActionStatusNoChange", "No change");
            case ActionStatus::Failed: return L("General", "sActionStatusFailed", "Failed / incomplete");
            }
            return std::string{};
        };
        const auto describe = [&](const ActionReceipt& receipt) {
            const auto& request = receipt.request;
            const auto name = receipt.outcome.targetName.empty() ? FormatUtils::FormID(request.formID) : receipt.outcome.targetName;
            switch (request.kind) {
            case ActionKind::RestoreHealth: return L("Inventory", "sRefillHealth", "Refill Health");
            case ActionKind::ToggleNoClip: return L("Inventory", "sToggleNoClip", "Toggle Noclip");
            case ActionKind::SetPlayerLevel: return L("Inventory", "sSetLevel", "Set Level") + " " + std::to_string(request.count);
            case ActionKind::AddPerkPoints: return L("Inventory", "sAddPerkPointsBtn", "Add Perk Points") + " " + std::to_string(request.count);
            case ActionKind::SetGameHour: return L("Inventory", "sTimeOfDaySection", "Time of Day") + " " + std::format("{:.2f}", request.value);
            case ActionKind::Give:
            case ActionKind::GiveWithAmmo: {
                auto text = L("General", "sActionGive", "Give") + " " + std::to_string(request.count) + " x " + name;
                if (request.ammoCount) text += " + " + std::to_string(request.ammoCount) + " " + L("Items", "sAmmo", "Ammo");
                return text;
            }
            case ActionKind::Spawn: return L("NPCs", "sSpawnNPC", "Spawn") + " " + std::to_string(request.count) + " x " + name;
            case ActionKind::Place: return L("General", "sActionPlace", "Place") + " " + std::to_string(request.count) + " x " + name;
            case ActionKind::AddSpell: return L("General", "sActionAddSpell", "Add spell") + " " + name;
            case ActionKind::RemoveSpell: return L("General", "sActionRemoveSpell", "Remove spell") + " " + name;
            case ActionKind::AddPerk: return L("General", "sActionAddPerk", "Add perk") + " " + name;
            case ActionKind::RemovePerk: return L("General", "sActionRemovePerk", "Remove perk") + " " + name;
            case ActionKind::Outfit: return L("General", "sActionGiveOutfitItems", "Give outfit items from") + " " + name;
            case ActionKind::ConstructedItem: return L("General", "sActionGiveCraftedItem", "Give crafted item from") + " " + name;
            case ActionKind::CurrentAmmo: return L("Player", "sCurrentWeaponAmmo", "Current Weapon Ammo") + " x " + std::to_string(request.count);
            case ActionKind::GodMode: return request.ammoCount ? L("Player", "sGodModeOn", "Godmode: ON") : L("Player", "sGodModeOff", "Godmode: OFF");
            case ActionKind::Console: return L("General", "sActionCommand", "Console command") + ": " + request.command;
            case ActionKind::Teleport: return L("General", "sActionTeleportTo", "Teleport to") + " " + name;
            case ActionKind::Equip: return L("General", "sEquipItem", "Equip Item") + " " + name;
            case ActionKind::StartQuest: return L("General", "sStartQuest", "Start Quest") + " " + name;
            case ActionKind::CompleteQuest: return L("General", "sCompleteQuest", "Complete Quest") + " " + name;
            case ActionKind::SetWeather: return L("General", "sSetWeather", "Set Weather") + " " + name;
            case ActionKind::PlaySound: return L("General", "sPlaySound", "Play Sound") + " " + name;
            case ActionKind::SetGlobal: return L("General", "sSetGlobal", "Set Global") + " " + name + " = " + std::format("{:.3f}", request.value);
            case ActionKind::InventoryRemove: return L("Inventory", "sRemoveItem", "Remove Item") + " " + name + " x" + std::to_string(request.count);
            case ActionKind::InventoryDrop: return L("Inventory", "sDropItem", "Drop Item") + " " + name + " x" + std::to_string(request.count);
            case ActionKind::InventoryEquip: return L("Inventory", "sEquipItem", "Equip") + " " + name;
            case ActionKind::InventoryUnequip: return L("Inventory", "sUnequipItem", "Unequip") + " " + name;
            case ActionKind::InventoryUse: return L("Inventory", "sUseItem", "Use") + " " + name;
            case ActionKind::InventoryDuplicateWeapon: return L("Inventory", "sDuplicateWeapon", "Duplicate Weapon") + " " + name;
            case ActionKind::InventoryAddBase: return L("Inventory", "sAddBaseItem", "Add Base Item") + " " + name + " x" + std::to_string(request.count);
            }
            return name;
        };
        std::vector<ActionHistoryEntry> result;
        result.reserve(receipts.size());
        for (auto it = receipts.rbegin(); it != receipts.rend(); ++it) {
            ActionHistoryEntry entry{ .id = it->id, .description = describe(*it), .status = statusLabel(it->outcome.status) };
            entry.details = InventoryFeedback::Message(it->outcome.inventoryRejection, L);
            for (const auto& effect : it->outcome.effects) {
                if (!entry.details.empty()) entry.details += "\n";
                entry.details += (effect.name.empty() ? FormatUtils::FormID(effect.formID) : effect.name) + ": " + statusLabel(effect.status);
                if (effect.before && effect.after) entry.details += " (" + std::to_string(*effect.before) + " -> " + std::to_string(*effect.after) + ")";
            }
            result.push_back(std::move(entry));
        }
        return result;
    }
}
