#include "pch.h"
#include "Game/ActionExecutor.h"
#include "Game/InventoryRuntime.h"
#include "Core/ObserveAction.h"

#include <RE/B/BGSComponent.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSEquipIndex.h>
#include <RE/B/BGSOutfit.h>
#include <RE/B/BGSPerk.h>
#include <RE/C/Console.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/S/SpellItem.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESObjectARMO.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESQuest.h>
#include <RE/T/TESWeather.h>
#include <RE/T/TESGlobal.h>

#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        std::string DisplayName(RE::TESForm* form)
        {
            if (!form) return {};
            const auto name = RE::TESFullName::GetFullName(*form);
            if (!name.empty()) return std::string(name);
            const auto* editorID = form->GetFormEditorID();
            return editorID ? std::string(editorID) : std::string{};
        }

        RE::TESForm* Resolve(std::uint32_t formID, bool substitute)
        {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (!form || form->IsDeleted()) return nullptr;
            if (substitute) {
                if (auto* component = form->As<RE::BGSComponent>(); component && component->scrapItem) {
                    form = component->scrapItem;
                }
            }
            return form->IsDeleted() ? nullptr : form;
        }

        std::string PlayerCommand(const char* command, std::uint32_t formID, std::optional<std::uint32_t> count = {})
        {
            char text[128]{};
            if (count) std::snprintf(text, sizeof(text), "player.%s %08X %u", command, formID, *count);
            else std::snprintf(text, sizeof(text), "player.%s %08X", command, formID);
            return text;
        }

        std::optional<std::int64_t> UnknownState() { return std::nullopt; }
    }

    bool ActionExecutor::AreGameplayActionsAllowed(bool debugOverride)
    {
        auto* ui = RE::UI::GetSingleton();
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!ui || !player || !player->GetParentCell()) return false;
        static const RE::BSFixedString loadingMenu("LoadingMenu");
        if (ui->GetMenuOpen(loadingMenu)) return false;
        return !ui->GetMenuOpen<RE::MainMenu>() || debugOverride;
    }

    std::uint32_t ActionExecutor::GetWeaponAmmoFormID(std::uint32_t weaponFormID)
    {
        auto* form = Resolve(weaponFormID, false);
        const auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
        return weapon && weapon->weaponData.ammo ? weapon->weaponData.ammo->GetFormID() : 0;
    }

    bool ActionExecutor::IsPlayerGodModeEnabled()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && player->IsGodMode();
    }

    ActionOutcome ActionExecutor::Execute(const ActionRequest& request, const std::function<bool()>& isCurrent)
    {
        ActionOutcome outcome;
        const auto ready = [&] { return isCurrent() && AreGameplayActionsAllowed(request.debugOverride); };
        if (!ActionQueue::Valid(request) || !ready()) return outcome;
        if (InventoryOperation(request.kind) || request.kind == ActionKind::InventoryAddBase) return InventoryRuntime::Execute(request, isCurrent);
        auto* target = request.formID ? Resolve(request.formID, false) : nullptr;
        if (request.formID && !target) return outcome;
        outcome.targetName = DisplayName(target);

        const auto dispatch = [&](const std::string& command) {
            if (!ready() || command.empty()) return false;
            RE::Console::ExecuteCommand(command.c_str());
            return true;
        };
        const auto give = [&](std::uint32_t formID, std::uint32_t count) {
            ActionEffect effect{ .formID = formID, .count = count };
            if (!ready() || count == 0 || count > ActionQueue::MaxQuantity) return effect;
            auto* form = Resolve(formID, request.substituteComponents);
            auto* object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (!object) return effect;
            effect.formID = object->GetFormID();
            effect.name = DisplayName(object);
            auto* player = RE::PlayerCharacter::GetSingleton();
            const auto read = [&]() -> std::optional<std::int64_t> { return player->GetInventoryObjectCount(object); };
            return ObserveAction(std::move(effect), read, [&] {
                player->AddInventoryItem(object, nullptr, count, nullptr, nullptr, nullptr);
                return true;
            }, ready);
        };
        const auto commandEffect = [&](const std::string& command, RE::TESForm* form = nullptr) {
            return ObserveAction(ActionEffect{ .formID = form ? form->GetFormID() : request.formID,
                .count = request.count, .name = DisplayName(form) }, UnknownState,
                [&] { return dispatch(command); }, ready);
        };

        switch (request.kind) {
        case ActionKind::RestoreHealth: outcome.effects.push_back(commandEffect("player.resethealth")); break;
        case ActionKind::ToggleNoClip: outcome.effects.push_back(commandEffect("tcl")); break;
        case ActionKind::SetPlayerLevel: outcome.effects.push_back(commandEffect(std::format("player.setlevel {}", request.count))); break;
        case ActionKind::AddPerkPoints: outcome.effects.push_back(commandEffect(std::format("cgf \"Game.AddPerkPoints\" {}", request.count))); break;
        case ActionKind::SetGameHour: outcome.effects.push_back(commandEffect(std::format("set gamehour to {:.2f}", request.value))); break;
        case ActionKind::Give:
            outcome.effects.push_back(give(request.formID, request.count));
            break;
        case ActionKind::GiveWithAmmo:
            outcome.effects.push_back(give(request.formID, request.count));
            if (request.ammoCount > 0) outcome.effects.push_back(give(request.ammoFormID, request.ammoCount));
            break;
        case ActionKind::Equip:
            if (target->As<RE::TESObjectWEAP>() || target->As<RE::TESObjectARMO>()) {
                outcome.effects.push_back(commandEffect(PlayerCommand("equipitem", request.formID), target));
                if (ready() && request.ammoCount) {
                    if (const auto ammo = GetWeaponAmmoFormID(request.formID)) outcome.effects.push_back(give(ammo, request.ammoCount));
                }
            }
            break;
        case ActionKind::Spawn:
        case ActionKind::Place: {
            auto* form = Resolve(request.formID, request.substituteComponents);
            if (form && (form->As<RE::TESBoundObject>() || form->As<RE::TESNPC>())) {
                outcome.effects.push_back(commandEffect(PlayerCommand("placeatme", form->GetFormID(), request.count), form));
            }
            break;
        }
        case ActionKind::AddSpell:
        case ActionKind::RemoveSpell: {
            auto* spell = target->As<RE::SpellItem>();
            if (!spell) break;
            auto* player = RE::PlayerCharacter::GetSingleton();
            // Absence from addedSpells cannot rule out base/race/leveled sources.
            const auto read = [&]() -> std::optional<std::int64_t> {
                return std::ranges::find(player->addedSpells, spell) != player->addedSpells.end() ?
                    std::optional<std::int64_t>{ 1 } : std::nullopt;
            };
            const bool add = request.kind == ActionKind::AddSpell;
            outcome.effects.push_back(ObserveAction(ActionEffect{ .formID = request.formID, .name = outcome.targetName }, read,
                [&] { return dispatch(PlayerCommand(add ? "addspell" : "removespell", request.formID)); }, ready,
                add ? std::optional<std::int64_t>{ 1 } : std::nullopt));
            break;
        }
        case ActionKind::AddPerk:
        case ActionKind::RemovePerk: {
            auto* perk = target->As<RE::BGSPerk>();
            if (!perk) break;
            auto* player = RE::PlayerCharacter::GetSingleton();
            const auto read = [&]() -> std::optional<std::int64_t> { return player->GetPerkRank(perk); };
            outcome.effects.push_back(ObserveAction(ActionEffect{ .formID = request.formID, .name = outcome.targetName }, read,
                [&] { return dispatch(PlayerCommand(request.kind == ActionKind::AddPerk ? "addperk" : "removeperk", request.formID)); }, ready));
            break;
        }
        case ActionKind::Outfit: {
            const auto* outfit = target->As<RE::BGSOutfit>();
            if (!outfit || outfit->outfitItems.size() > ActionQueue::Capacity) break;
            // Detach IDs before dispatch, which can reenter lifecycle callbacks.
            std::vector<std::uint32_t> items;
            for (const auto* item : outfit->outfitItems) if (item) items.push_back(item->GetFormID());
            for (const auto formID : items) outcome.effects.push_back(give(formID, 1));
            break;
        }
        case ActionKind::ConstructedItem: {
            auto* recipe = target->As<RE::BGSConstructibleObject>();
            auto* item = recipe ? recipe->GetCreatedItem() : nullptr;
            if (item) outcome.effects.push_back(give(item->GetFormID(), 1));
            break;
        }
        case ActionKind::CurrentAmmo: {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* ammo = player->GetCurrentAmmo(RE::BGSEquipIndex{ 0 });
            std::uint32_t ammoID = ammo ? ammo->GetFormID() : 0;
            if (!ammoID) {
                RE::BGSObjectInstance equipped{ nullptr, nullptr };
                player->GetEquippedItem(&equipped, RE::BGSEquipIndex{ 0 });
                if (equipped.object) ammoID = GetWeaponAmmoFormID(equipped.object->GetFormID());
            }
            if (ammoID) outcome.effects.push_back(give(ammoID, request.count));
            break;
        }
        case ActionKind::GodMode:
            outcome.effects.push_back(ObserveAction(ActionEffect{},
                []() -> std::optional<std::int64_t> { return IsPlayerGodModeEnabled() ? 1 : 0; },
                [&] { return dispatch("tgm"); }, ready, request.ammoCount != 0 ? 1 : 0));
            break;
        case ActionKind::Console:
            outcome.effects.push_back(commandEffect(request.command));
            break;
        case ActionKind::Teleport:
            if (target->As<RE::TESObjectCELL>()) {
                const auto* editorID = target->GetFormEditorID();
                if (editorID && *editorID) outcome.effects.push_back(commandEffect(std::string("coc ") + editorID, target));
            }
            break;
        case ActionKind::StartQuest:
        case ActionKind::CompleteQuest:
            if (target->As<RE::TESQuest>()) {
                outcome.effects.push_back(commandEffect(std::format("{} {:08X}",
                    request.kind == ActionKind::StartQuest ? "startquest" : "completequest", request.formID), target));
            }
            break;
        case ActionKind::SetWeather:
            if (target->As<RE::TESWeather>()) outcome.effects.push_back(commandEffect(std::format("fw {:08X}", request.formID), target));
            break;
        case ActionKind::PlaySound:
        case ActionKind::SetGlobal: {
            const bool validType = request.kind == ActionKind::SetGlobal ? target->As<RE::TESGlobal>() != nullptr :
                target->Is(RE::ENUM_FORM_ID::kSOUN, RE::ENUM_FORM_ID::kSNDR);
            const auto* editorID = validType ? target->GetFormEditorID() : nullptr;
            if (editorID && *editorID) {
                const auto command = request.kind == ActionKind::SetGlobal ? std::format("set {} to {:.3f}", editorID, request.value) :
                    std::string("playsound ") + editorID;
                outcome.effects.push_back(commandEffect(command, target));
            }
            break;
        }
        }
        outcome.Finish();
        return outcome;
    }
}
