#include "GUI/Widgets/FormActions.h"

#include "Logging/Logger.h"

#include <imgui.h>

#include <RE/B/BGSEquipIndex.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSOutfit.h>
#include <RE/C/Console.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectWEAP.h>

#include <cstdio>
#include <deque>
#include <vector>

namespace ESPExplorerAE
{
    namespace
    {
        using UndoEntry = std::vector<std::string>;

        std::deque<UndoEntry> undoHistory{};
        UndoEntry undoBatch{};
        bool undoBatchActive{ false };
        constexpr std::size_t kMaxUndoEntries = 64;
        constexpr std::uint32_t kRightHandEquipIndex = 0;

        void PushUndoEntry(UndoEntry&& entry)
        {
            if (entry.empty()) {
                return;
            }

            undoHistory.emplace_back(std::move(entry));
            while (undoHistory.size() > kMaxUndoEntries) {
                undoHistory.pop_front();
            }
        }

        void RecordUndoCommand(std::string command)
        {
            if (command.empty()) {
                return;
            }

            if (undoBatchActive) {
                undoBatch.push_back(std::move(command));
                return;
            }

            UndoEntry entry{};
            entry.push_back(std::move(command));
            PushUndoEntry(std::move(entry));
        }

        void BeginUndoBatch()
        {
            undoBatch.clear();
            undoBatchActive = true;
        }

        void CommitUndoBatch()
        {
            if (!undoBatchActive) {
                return;
            }

            PushUndoEntry(std::move(undoBatch));
            undoBatch.clear();
            undoBatchActive = false;
        }

        void CancelUndoBatch()
        {
            undoBatch.clear();
            undoBatchActive = false;
        }

        void ExecutePlayerCommand(const char* commandName, std::uint32_t formID, std::uint32_t count)
        {
            char command[128]{};
            std::snprintf(command, sizeof(command), "player.%s %08X %u", commandName, formID, count);
            Logger::Verbose(std::string("Execute command: ") + command);
            RE::Console::ExecuteCommand(command);
        }

        void ExecutePlayerCommand(const char* commandName, std::uint32_t formID)
        {
            char command[96]{};
            std::snprintf(command, sizeof(command), "player.%s %08X", commandName, formID);
            Logger::Verbose(std::string("Execute command: ") + command);
            RE::Console::ExecuteCommand(command);
        }
    }

    void FormActions::CopyFormID(std::uint32_t formID)
    {
        char formIDBuffer[16]{};
        std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", formID);
        ImGui::SetClipboardText(formIDBuffer);
        Logger::Verbose(std::string("Copied FormID: ") + formIDBuffer);
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
        Logger::Verbose("Gave item to player form=" + std::to_string(formID) + " count=" + std::to_string(count));

        char undoCommand[128]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removeitem %08X %u", formID, count);
        RecordUndoCommand(undoCommand);
    }

    void FormActions::GiveToPlayerWithAmmo(std::uint32_t formID, std::uint32_t itemCount, std::uint32_t ammoFormID, std::uint32_t ammoCount)
    {
        BeginUndoBatch();
        GiveToPlayer(formID, itemCount);

        if (ammoFormID != 0 && ammoCount > 0) {
            GiveToPlayer(ammoFormID, ammoCount);
        }
        CommitUndoBatch();
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
        Logger::Verbose("Added ammo for current weapon count=" + std::to_string(ammoCount));
        return true;
    }

    void FormActions::SpawnAtPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        ExecutePlayerCommand("placeatme", formID, count);
        Logger::Verbose("Spawn at player form=" + std::to_string(formID) + " count=" + std::to_string(count));
    }

    void FormActions::PlaceAtPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        ExecutePlayerCommand("placeatme", formID, count);
        Logger::Verbose("Place at player form=" + std::to_string(formID) + " count=" + std::to_string(count));
    }

    void FormActions::AddSpellToPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("addspell", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removespell %08X", formID);
        RecordUndoCommand(undoCommand);
    }

    void FormActions::RemoveSpellFromPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("removespell", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.addspell %08X", formID);
        RecordUndoCommand(undoCommand);
    }

    void FormActions::AddPerkToPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("addperk", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removeperk %08X", formID);
        RecordUndoCommand(undoCommand);
    }

    void FormActions::RemovePerkFromPlayer(std::uint32_t formID)
    {
        ExecutePlayerCommand("removeperk", formID);

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.addperk %08X", formID);
        RecordUndoCommand(undoCommand);
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

        BeginUndoBatch();
        int addedCount = 0;
        for (const auto& item : outfit->outfitItems) {
            if (!item) {
                continue;
            }

            GiveToPlayer(item->GetFormID(), 1);
            ++addedCount;
        }

        if (addedCount > 0) {
            CommitUndoBatch();
        } else {
            CancelUndoBatch();
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

        Logger::Verbose(std::string("Execute console command: ") + std::string(command));
        RE::Console::ExecuteCommand(std::string(command).c_str());
    }

    void FormActions::TeleportToCell(std::uint32_t formID)
    {
        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            return;
        }

        const auto* editorID = form->GetFormEditorID();
        if (!editorID || editorID[0] == '\0') {
            return;
        }

        std::string command = std::string("coc ") + editorID;
        ExecuteConsoleCommand(command);
    }

    bool FormActions::CanUndoLastAction()
    {
        return !undoHistory.empty();
    }

    void FormActions::UndoLastAction()
    {
        if (undoHistory.empty()) {
            return;
        }

        auto entry = std::move(undoHistory.back());
        undoHistory.pop_back();

        for (auto it = entry.rbegin(); it != entry.rend(); ++it) {
            RE::Console::ExecuteCommand(it->c_str());
            Logger::Verbose(std::string("Undo command executed: ") + *it);
        }
    }
}
