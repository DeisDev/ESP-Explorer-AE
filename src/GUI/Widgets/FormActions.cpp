#include "GUI/Widgets/FormActions.h"

#include "Config/Config.h"
#include "GUI/Widgets/FormatUtils.h"
#include "Localization/Language.h"

#include <imgui.h>

#include <RE/B/BGSComponent.h>
#include <RE/B/BGSEquipIndex.h>
#include <RE/B/BGSConstructibleObject.h>
#include <RE/B/BGSOutfit.h>
#include <RE/C/Console.h>
#include <RE/P/PlayerCharacter.h>
#include <RE/T/TESFullName.h>
#include <RE/T/TESObjectWEAP.h>
#include <RE/T/TESObjectCELL.h>
#include <RE/T/TESBoundObject.h>
#include <RE/T/TESObjectWEAP.h>

#include <algorithm>
#include <cstdio>
#include <deque>
#include <vector>

namespace ESPExplorerAE
{
    namespace
    {
        struct ActionHistoryEntry
        {
            std::uint64_t id{ 0 };
            std::string description;
            std::vector<std::string> undoCommands{};
        };

        struct PendingGiveEntry
        {
            std::uint32_t formID{ 0 };
            std::uint32_t itemCount{ 0 };
            std::uint32_t ammoFormID{ 0 };
            std::uint32_t ammoCount{ 0 };
        };

        std::deque<ActionHistoryEntry> actionHistory{};
        std::deque<PendingGiveEntry> pendingGiveEntries{};
        ActionHistoryEntry actionBatch{};
        bool actionBatchActive{ false };
        std::uint64_t nextActionID{ 1 };
        constexpr std::size_t kMaxUndoEntries = 64;
        constexpr std::size_t kDefaultPendingGiveActionsPerFrame = 8;
        constexpr std::uint32_t kRightHandEquipIndex = 0;

        std::string LocalizeWithFallback(std::string_view section, std::string_view key, std::string_view fallback)
        {
            const auto localized = Language::Get(section, key);
            return localized.empty() ? std::string(fallback) : std::string(localized);
        }

        std::string ResolveFormDisplayName(std::uint32_t formID)
        {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (!form) {
                return FormatUtils::FormID(formID);
            }

            const auto fullName = RE::TESFullName::GetFullName(*form);
            if (!fullName.empty()) {
                return std::string(fullName);
            }

            const auto* editorID = form->GetFormEditorID();
            if (editorID && editorID[0] != '\0') {
                return std::string(editorID);
            }

            return FormatUtils::FormID(formID);
        }

        std::string BuildGiveDescription(std::uint32_t formID, std::uint32_t count)
        {
            return LocalizeWithFallback("General", "sActionGive", "Give") + " " + std::to_string(count) + " x " + ResolveFormDisplayName(formID);
        }

        std::string BuildGiveWithAmmoDescription(std::uint32_t formID, std::uint32_t itemCount, std::uint32_t ammoFormID, std::uint32_t ammoCount)
        {
            std::string description = BuildGiveDescription(formID, itemCount);
            if (ammoFormID != 0 && ammoCount > 0) {
                description += " + " + std::to_string(ammoCount) + " " + LocalizeWithFallback("Items", "sAmmo", "ammo") + " (" + ResolveFormDisplayName(ammoFormID) + ")";
            }
            return description;
        }

        std::string BuildSpawnDescription(std::string_view action, std::uint32_t formID, std::uint32_t count)
        {
            return std::string(action) + " " + std::to_string(count) + " x " + ResolveFormDisplayName(formID);
        }

        std::string BuildTeleportDescription(std::uint32_t formID)
        {
            return LocalizeWithFallback("General", "sActionTeleportTo", "Teleport to") + " " + ResolveFormDisplayName(formID);
        }

        std::string BuildSpellDescription(std::string_view action, std::uint32_t formID)
        {
            return std::string(action) + " " + ResolveFormDisplayName(formID);
        }

        std::string BuildOutfitDescription(std::uint32_t formID)
        {
            return LocalizeWithFallback("General", "sActionGiveOutfitItems", "Give outfit items from") + " " + ResolveFormDisplayName(formID);
        }

        std::string BuildCraftedItemDescription(std::uint32_t formID)
        {
            return LocalizeWithFallback("General", "sActionGiveCraftedItem", "Give crafted item from") + " " + ResolveFormDisplayName(formID);
        }

        std::string BuildTeleportUndoCommand(std::string_view editorID)
        {
            return std::string("coc ") + std::string(editorID);
        }

        std::string GetCurrentCellEditorID()
        {
            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return {};
            }

            const auto* parentCell = player->GetParentCell();
            if (!parentCell) {
                return {};
            }

            const auto* editorID = parentCell->GetFormEditorID();
            if (!editorID || editorID[0] == '\0') {
                return {};
            }

            return std::string(editorID);
        }

        void PushHistoryEntry(ActionHistoryEntry&& entry)
        {
            if (entry.description.empty() && entry.undoCommands.empty()) {
                return;
            }

            entry.id = nextActionID++;
            actionHistory.emplace_back(std::move(entry));
            while (actionHistory.size() > kMaxUndoEntries) {
                actionHistory.pop_front();
            }
        }

        void RecordUndoCommand(std::string command)
        {
            if (command.empty()) {
                return;
            }

            if (actionBatchActive) {
                actionBatch.undoCommands.push_back(std::move(command));
            }
        }

        void RecordAction(std::string description, std::vector<std::string> undoCommands = {})
        {
            ActionHistoryEntry entry{};
            entry.description = std::move(description);
            entry.undoCommands = std::move(undoCommands);
            PushHistoryEntry(std::move(entry));
        }

        void BeginActionBatch(std::string description)
        {
            actionBatch = ActionHistoryEntry{};
            actionBatch.description = std::move(description);
            actionBatchActive = true;
        }

        void CommitActionBatch()
        {
            if (!actionBatchActive) {
                return;
            }

            PushHistoryEntry(std::move(actionBatch));
            actionBatch = ActionHistoryEntry{};
            actionBatchActive = false;
        }

        void CancelActionBatch()
        {
            actionBatch = ActionHistoryEntry{};
            actionBatchActive = false;
        }

        bool GiveToPlayerInternal(std::uint32_t formID, std::uint32_t count, std::string* undoCommand = nullptr)
        {
            if (!FormActions::AreGameplayActionsAllowed() || count == 0) {
                return false;
            }

            auto* form = RE::TESForm::GetFormByID(formID);
            if (!form) {
                return false;
            }

            auto* object = form->As<RE::TESBoundObject>();
            if (!object) {
                return false;
            }

            if (Config::Get().componentSubstitution) {
                if (auto* component = form->As<RE::BGSComponent>()) {
                    if (component->scrapItem) {
                        const auto originalID = formID;
                        object = component->scrapItem;
                        formID = object->GetFormID();
                        char buf[128]{};
                        std::snprintf(buf, sizeof(buf), "Component substitution: CMPO %08X -> MISC %08X", originalID, formID);
                        REX::INFO("{}", std::string(buf));
                    }
                }
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                return false;
            }

            player->AddInventoryItem(object, nullptr, count, nullptr, nullptr, nullptr);
            REX::DEBUG("{}", "Gave item to player form=" + std::to_string(formID) + " count=" + std::to_string(count));

            if (undoCommand) {
                char command[128]{};
                std::snprintf(command, sizeof(command), "player.removeitem %08X %u", formID, count);
                *undoCommand = command;
            }

            return true;
        }

        bool UndoHistoryEntry(std::deque<ActionHistoryEntry>::iterator it)
        {
            if (it == actionHistory.end() || it->undoCommands.empty()) {
                return false;
            }

            for (auto commandIt = it->undoCommands.rbegin(); commandIt != it->undoCommands.rend(); ++commandIt) {
                if (!FormActions::ExecuteConsoleCommand(*commandIt)) {
                    return false;
                }
                REX::DEBUG("{}", std::string("Undo command executed: ") + *commandIt);
            }

            actionHistory.erase(it);
            return true;
        }

        bool ExecutePlayerCommand(const char* commandName, std::uint32_t formID, std::uint32_t count)
        {
            if (!FormActions::AreGameplayActionsAllowed()) {
                return false;
            }

            char command[128]{};
            std::snprintf(command, sizeof(command), "player.%s %08X %u", commandName, formID, count);
            REX::DEBUG("{}", std::string("Execute command: ") + command);
            RE::Console::ExecuteCommand(command);
            return true;
        }

        bool ExecutePlayerCommand(const char* commandName, std::uint32_t formID)
        {
            if (!FormActions::AreGameplayActionsAllowed()) {
                return false;
            }

            char command[96]{};
            std::snprintf(command, sizeof(command), "player.%s %08X", commandName, formID);
            REX::DEBUG("{}", std::string("Execute command: ") + command);
            RE::Console::ExecuteCommand(command);
            return true;
        }
    }

    void FormActions::CopyFormID(std::uint32_t formID)
    {
        const std::string formIDText = FormatUtils::FormID(formID);
        ImGui::SetClipboardText(formIDText.c_str());
        REX::DEBUG("{}", std::string("Copied FormID: ") + formIDText);
    }

    void FormActions::GiveToPlayer(std::uint32_t formID, std::uint32_t count)
    {
        std::string undoCommand{};
        if (!GiveToPlayerInternal(formID, count, &undoCommand)) {
            return;
        }

        RecordAction(BuildGiveDescription(formID, count), { std::move(undoCommand) });
    }

    void FormActions::GiveToPlayerWithAmmo(std::uint32_t formID, std::uint32_t itemCount, std::uint32_t ammoFormID, std::uint32_t ammoCount)
    {
        BeginActionBatch(BuildGiveWithAmmoDescription(formID, itemCount, ammoFormID, ammoCount));

        bool anyAdded = false;
        std::string undoCommand{};
        if (GiveToPlayerInternal(formID, itemCount, &undoCommand)) {
            RecordUndoCommand(std::move(undoCommand));
            anyAdded = true;
        }

        if (ammoFormID != 0 && ammoCount > 0) {
            undoCommand.clear();
            if (GiveToPlayerInternal(ammoFormID, ammoCount, &undoCommand)) {
                RecordUndoCommand(std::move(undoCommand));
                anyAdded = true;
            }
        }

        if (anyAdded) {
            CommitActionBatch();
        } else {
            CancelActionBatch();
        }
    }

    void FormActions::QueueGiveToPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (!AreGameplayActionsAllowed() || formID == 0 || count == 0) {
            return;
        }

        pendingGiveEntries.push_back(PendingGiveEntry{
            .formID = formID,
            .itemCount = count,
            .ammoFormID = 0,
            .ammoCount = 0
        });
    }

    void FormActions::QueueGiveToPlayerWithAmmo(std::uint32_t formID, std::uint32_t itemCount, std::uint32_t ammoFormID, std::uint32_t ammoCount)
    {
        if (!AreGameplayActionsAllowed() || formID == 0 || itemCount == 0) {
            return;
        }

        pendingGiveEntries.push_back(PendingGiveEntry{
            .formID = formID,
            .itemCount = itemCount,
            .ammoFormID = ammoFormID,
            .ammoCount = ammoCount
        });
    }

    void FormActions::ProcessPendingActions(std::size_t maxActions)
    {
        if (!AreGameplayActionsAllowed()) {
            return;
        }

        const std::size_t budget = maxActions == 0 ? kDefaultPendingGiveActionsPerFrame : maxActions;
        std::size_t processedCount = 0;

        while (processedCount < budget && !pendingGiveEntries.empty()) {
            const PendingGiveEntry entry = pendingGiveEntries.front();
            pendingGiveEntries.pop_front();

            if (entry.ammoFormID != 0 && entry.ammoCount > 0) {
                GiveToPlayerWithAmmo(entry.formID, entry.itemCount, entry.ammoFormID, entry.ammoCount);
            } else {
                GiveToPlayer(entry.formID, entry.itemCount);
            }

            ++processedCount;
        }
    }

    std::size_t FormActions::GetPendingGiveCount()
    {
        return pendingGiveEntries.size();
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
        if (ammo) {
            GiveToPlayer(ammo->GetFormID(), ammoCount);
            REX::DEBUG("{}", "Added ammo for current weapon count=" + std::to_string(ammoCount));
            return true;
        }

        RE::BGSObjectInstance equippedObj{ nullptr, nullptr };
        player->GetEquippedItem(&equippedObj, RE::BGSEquipIndex{ kRightHandEquipIndex });
        if (equippedObj.object) {
            auto* weapon = equippedObj.object->As<RE::TESObjectWEAP>();
            if (weapon && weapon->weaponData.ammo) {
                GiveToPlayer(weapon->weaponData.ammo->GetFormID(), ammoCount);
                REX::DEBUG("{}", "Added ammo for current weapon (from weapon data) count=" + std::to_string(ammoCount));
                return true;
            }
        }

        return false;
    }

    void FormActions::SpawnAtPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        if (Config::Get().componentSubstitution) {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (auto* component = form ? form->As<RE::BGSComponent>() : nullptr) {
                if (component->scrapItem) {
                    const auto originalID = formID;
                    formID = component->scrapItem->GetFormID();
                    char buf[128]{};
                    std::snprintf(buf, sizeof(buf), "Component substitution (spawn): CMPO %08X -> MISC %08X", originalID, formID);
                    REX::INFO("{}", std::string(buf));
                }
            }
        }

        if (!ExecutePlayerCommand("placeatme", formID, count)) {
            return;
        }

        REX::DEBUG("{}", "Spawn at player form=" + std::to_string(formID) + " count=" + std::to_string(count));
        RecordAction(BuildSpawnDescription(LocalizeWithFallback("NPCs", "sSpawnNPC", "Spawn"), formID, count));
    }

    void FormActions::PlaceAtPlayer(std::uint32_t formID, std::uint32_t count)
    {
        if (count == 0) {
            return;
        }

        if (Config::Get().componentSubstitution) {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (auto* component = form ? form->As<RE::BGSComponent>() : nullptr) {
                if (component->scrapItem) {
                    const auto originalID = formID;
                    formID = component->scrapItem->GetFormID();
                    char buf[128]{};
                    std::snprintf(buf, sizeof(buf), "Component substitution (place): CMPO %08X -> MISC %08X", originalID, formID);
                    REX::INFO("{}", std::string(buf));
                }
            }
        }

        if (!ExecutePlayerCommand("placeatme", formID, count)) {
            return;
        }

        REX::DEBUG("{}", "Place at player form=" + std::to_string(formID) + " count=" + std::to_string(count));
        RecordAction(BuildSpawnDescription(LocalizeWithFallback("General", "sActionPlace", "Place"), formID, count));
    }

    void FormActions::AddSpellToPlayer(std::uint32_t formID)
    {
        if (!ExecutePlayerCommand("addspell", formID)) {
            return;
        }

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removespell %08X", formID);
        RecordAction(BuildSpellDescription(LocalizeWithFallback("General", "sActionAddSpell", "Add spell"), formID), { undoCommand });
    }

    void FormActions::RemoveSpellFromPlayer(std::uint32_t formID)
    {
        if (!ExecutePlayerCommand("removespell", formID)) {
            return;
        }

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.addspell %08X", formID);
        RecordAction(BuildSpellDescription(LocalizeWithFallback("General", "sActionRemoveSpell", "Remove spell"), formID), { undoCommand });
    }

    void FormActions::AddPerkToPlayer(std::uint32_t formID)
    {
        if (!ExecutePlayerCommand("addperk", formID)) {
            return;
        }

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.removeperk %08X", formID);
        RecordAction(BuildSpellDescription(LocalizeWithFallback("General", "sActionAddPerk", "Add perk"), formID), { undoCommand });
    }

    void FormActions::RemovePerkFromPlayer(std::uint32_t formID)
    {
        if (!ExecutePlayerCommand("removeperk", formID)) {
            return;
        }

        char undoCommand[96]{};
        std::snprintf(undoCommand, sizeof(undoCommand), "player.addperk %08X", formID);
        RecordAction(BuildSpellDescription(LocalizeWithFallback("General", "sActionRemovePerk", "Remove perk"), formID), { undoCommand });
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

        BeginActionBatch(BuildOutfitDescription(formID));
        int addedCount = 0;
        for (const auto& item : outfit->outfitItems) {
            if (!item) {
                continue;
            }

            std::string undoCommand{};
            if (GiveToPlayerInternal(item->GetFormID(), 1, &undoCommand)) {
                RecordUndoCommand(std::move(undoCommand));
                ++addedCount;
            }
        }

        if (addedCount > 0) {
            CommitActionBatch();
        } else {
            CancelActionBatch();
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

        std::string undoCommand{};
        if (!GiveToPlayerInternal(created->GetFormID(), 1, &undoCommand)) {
            return false;
        }

        RecordAction(BuildCraftedItemDescription(formID), { std::move(undoCommand) });
        return true;
    }

    bool FormActions::IsPlayerGodModeEnabled()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        return player && player->IsGodMode();
    }

    bool FormActions::AreGameplayActionsAllowed()
    {
        auto* ui = RE::UI::GetSingleton();
        const bool inMainMenu = ui && ui->GetMenuOpen<RE::MainMenu>();
        return !inMainMenu || Config::Get().allowGameplayActionsInMainMenu;
    }

    void FormActions::SetPlayerGodModeEnabled(bool enabled)
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return;
        }

        if (player->IsGodMode() == enabled) {
            return;
        }

        ExecuteConsoleCommand("tgm");
    }

    bool FormActions::ExecuteConsoleCommand(std::string_view command)
    {
        if (command.empty() || !AreGameplayActionsAllowed()) {
            return false;
        }

        REX::DEBUG("{}", std::string("Execute console command: ") + std::string(command));
        RE::Console::ExecuteCommand(std::string(command).c_str());
        return true;
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

        const auto previousCellEditorID = GetCurrentCellEditorID();
        std::string command = std::string("coc ") + editorID;
        if (!ExecuteConsoleCommand(command)) {
            return;
        }

        std::vector<std::string> undoCommands{};
        if (!previousCellEditorID.empty()) {
            undoCommands.push_back(BuildTeleportUndoCommand(previousCellEditorID));
        }

        RecordAction(BuildTeleportDescription(formID), std::move(undoCommands));
    }

    bool FormActions::CanUndoLastAction()
    {
        if (!AreGameplayActionsAllowed()) {
            return false;
        }

        return std::ranges::any_of(actionHistory, [](const ActionHistoryEntry& entry) {
            return !entry.undoCommands.empty();
        });
    }

    void FormActions::UndoLastAction()
    {
        for (auto it = actionHistory.end(); it != actionHistory.begin();) {
            --it;
            if (UndoHistoryEntry(it)) {
                return;
            }
        }
    }

    bool FormActions::UndoAction(std::uint64_t actionID)
    {
        const auto it = std::find_if(actionHistory.begin(), actionHistory.end(), [actionID](const ActionHistoryEntry& entry) {
            return entry.id == actionID;
        });

        return UndoHistoryEntry(it);
    }

    std::vector<FormActions::HistoryEntry> FormActions::GetRecentActionHistory()
    {
        std::vector<FormActions::HistoryEntry> history{};
        history.reserve(actionHistory.size());

        for (auto it = actionHistory.rbegin(); it != actionHistory.rend(); ++it) {
            history.push_back(FormActions::HistoryEntry{
                .id = it->id,
                .description = it->description,
                .canUndo = !it->undoCommands.empty()
            });
        }

        return history;
    }
}
