#include "GUI/Widgets/FormActions.h"

#include <imgui.h>

#include <RE/B/BGSEquipIndex.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSOutfit.h>
#include <RE/C/Console.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectWEAP.h>

#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        std::string lastUndoCommand{};
        constexpr std::uint32_t kRightHandEquipIndex = 0;

        void ExecutePlayerCommand(const char* commandName, std::uint32_t formID, std::uint32_t count)
        {
            char command[128]{};
            std::snprintf(command, sizeof(command), "player.%s %08X %u", commandName, formID, count);
            RE::Console::ExecuteCommand(command);
        }

        void ExecutePlayerCommand(const char* commandName, std::uint32_t formID)
        {
            char command[96]{};
            std::snprintf(command, sizeof(command), "player.%s %08X", commandName, formID);
            RE::Console::ExecuteCommand(command);
        }
    }

    void FormActions::CopyFormID(std::uint32_t formID)
    {
        char formIDBuffer[16]{};
        std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", formID);
        ImGui::SetClipboardText(formIDBuffer);
    }

    void FormActions::GiveToPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            return;
        }

        auto* object = form->As<RE::TESBoundObject>();
        if (!object) {
            return;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        player->AddInventoryItem(object, nullptr, count, nullptr, nullptr, nullptr);

        char undoCommand[128]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removeitem %08X %u", formID, count);
        lastUndoCommand = undoCommand;
    }

    void FormActions::GiveToPlayerWithAmmo(std::uint32_t formID, std::uint32_t itemCount, std::uint32_t ammoFormID, std::uint32_t ammoCount)
    {
        GiveToPlayer(formID, itemCount);

        if (ammoFormID != 0 && ammoCount > 0) {
            GiveToPlayer(ammoFormID, ammoCount);
        }
    }

    std::uint32_t FormActions::GetWeaponAmmoFormID(std::uint32_t weaponFormID)
    {
        auto* form = RE::TESForm::GetFormByID(weaponFormID);
        if (!form) {
            return 0;
        }

        auto* weapon = form->As<RE::TESObjectWEAP>();
        if (!weapon || !weapon->weaponData.ammo) {
            return 0;
        }

        return weapon->weaponData.ammo->GetFormID();
    }

    bool FormActions::AddAmmoForCurrentWeapon(std::uint32_t ammoCount)
    {
        if (ammoCount == 0) {
            return false;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        const auto ammo = player->GetCurrentAmmo(RE::BGSEquipIndex{ kRightHandEquipIndex });
        if (!ammo) {
            return false;
        }

        GiveToPlayer(ammo->GetFormID(), ammoCount);
        return true;
    }

    void FormActions::SpawnAtPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        ExecutePlayerCommand("placeatme", formID, count);
        lastUndoCommand.clear();
    }

    void FormActions::PlaceAtPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        ExecutePlayerCommand("placeatme", formID, count);
        lastUndoCommand.clear();
    }

    void FormActions::AddSpellToPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("addspell", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removespell %08X", formID);
        lastUndoCommand = undoCommand;
    }

    void FormActions::RemoveSpellFromPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("removespell", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.addspell %08X", formID);
        lastUndoCommand = undoCommand;
    }

    void FormActions::AddPerkToPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("addperk", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removeperk %08X", formID);
        lastUndoCommand = undoCommand;
    }

    void FormActions::RemovePerkFromPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("removeperk", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.addperk %08X", formID);
        lastUndoCommand = undoCommand;
    }

    int FormActions::AddOutfitItemsToPlayer(std::uint32_t formID)
    {
        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            return 0;
        }

        auto* outfit = form->As<RE::BGSOutfit>();
        if (!outfit) {
            return 0;
        }

        int addedCount = 0;
        for (const auto& item : outfit->outfitItems) {
            if (!item) {
                continue;
            }

            GiveToPlayer(item->GetFormID(), 1);
            ++addedCount;
        }

        return addedCount;
    }

    bool FormActions::AddConstructedItemToPlayer(std::uint32_t formID)
    {
        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            return false;
        }

        auto* constructible = form->As<RE::BGSConstructibleObject>();
        if (!constructible) {
            return false;
        }

        auto* created = constructible->GetCreatedItem();
        if (!created) {
            return false;
        }

        GiveToPlayer(created->GetFormID(), 1);
        return true;
    }

    void FormActions::ExecuteConsoleCommand(std::string_view command)
    {
        if (command.empty()) {
            return;
        }

        RE::Console::ExecuteCommand(std::string(command).c_str());
    }

    bool FormActions::CanUndoLastAction()
    {
        return !lastUndoCommand.empty();
    }

    void FormActions::UndoLastAction()
    {
        if (lastUndoCommand.empty()) {
            return;
        }

        RE::Console::ExecuteCommand(lastUndoCommand.c_str());
        lastUndoCommand.clear();
    }
}
