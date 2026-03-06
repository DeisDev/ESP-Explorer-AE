#include "GUI/Widgets/ContextMenu.h"

#include "GUI/Widgets/FormActions.h"

#include <imgui.h>

#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        std::unordered_map<std::uint32_t, int> spawnQuantities{};
    }

    bool ContextMenu::CanGiveItem(std::string_view category)
    {
        return category == "Weapon" || category == "Armor" || category == "Ammo" || category == "Misc" ||
               category == "WEAP" || category == "ARMO" || category == "AMMO" || category == "MISC" ||
               category == "ALCH" || category == "BOOK" || category == "KEYM" || category == "NOTE" ||
               category == "INGR" || category == "CMPO" || category == "OMOD";
    }

    bool ContextMenu::CanSpawn(std::string_view category)
    {
        return category == "NPC" || category == "NPC_" || category == "LVLN" ||
               category == "Activator" || category == "Container" || category == "Static" || category == "Furniture" ||
               category == "ACTI" || category == "CONT" || category == "STAT" || category == "FURN" ||
               category == "LIGH" || category == "FLOR" || category == "TREE";
    }

    bool ContextMenu::CanTeleport(std::string_view category)
    {
        return category == "CELL" || category == "WRLD" || category == "LCTN" || category == "REGN";
    }

    bool ContextMenu::IsEquippable(std::string_view category)
    {
        return category == "WEAP" || category == "Weapon" || category == "ARMO" || category == "Armor";
    }

    bool ContextMenu::IsSpellLike(std::string_view category)
    {
        return category == "SPEL" || category == "Spell" || category == "MGEF" || category == "Effect";
    }

    bool ContextMenu::IsPerk(std::string_view category)
    {
        return category == "PERK" || category == "Perk";
    }

    bool ContextMenu::IsQuest(std::string_view category)
    {
        return category == "QUST" || category == "Quest";
    }

    bool ContextMenu::IsWeather(std::string_view category)
    {
        return category == "WTHR" || category == "Weather";
    }

    bool ContextMenu::IsSound(std::string_view category)
    {
        return category == "SOUN" || category == "SNDR";
    }

    bool ContextMenu::IsGlobal(std::string_view category)
    {
        return category == "GLOB";
    }

    bool ContextMenu::IsOutfit(std::string_view category)
    {
        return category == "OTFT";
    }

    bool ContextMenu::IsConstructible(std::string_view category)
    {
        return category == "COBJ";
    }

    const char* ContextMenu::TryGetEditorID(std::uint32_t formID)
    {
        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            return nullptr;
        }

        const auto* editorID = form->GetFormEditorID();
        if (!editorID || editorID[0] == '\0') {
            return nullptr;
        }

        return editorID;
    }

    void ContextMenu::Draw(const FormEntry& entry, const ContextMenuCallbacks& callbacks)
    {
        const auto L = [&](std::string_view section, std::string_view key, const char* fallback) -> const char* {
            if (callbacks.localize) {
                return callbacks.localize(section, key, fallback);
            }
            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        };

        const auto* displayName = entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : entry.name.c_str();
        const auto* editorID = TryGetEditorID(entry.formID);
        const bool canSpawnEntry = callbacks.canSpawnEntry ? callbacks.canSpawnEntry(entry) : CanSpawn(entry.category);

        ImGui::TextUnformatted(displayName);
        ImGui::TextDisabled("%08X  |  %s  |  %s", entry.formID, entry.sourcePlugin.c_str(), entry.category.c_str());
        if (editorID) {
            ImGui::TextDisabled("%s: %s", L("General", "sEditorID", "EditorID"), editorID);
        }

        ImGui::Separator();

        if (CanGiveItem(entry.category)) {
            if (ImGui::MenuItem(L("Items", "sGiveItem", "Give Item"))) {
                if (callbacks.openItemGrantPopup) {
                    callbacks.openItemGrantPopup(entry);
                }
            }
        }

        if (callbacks.showSpawnAtPlayer && (CanGiveItem(entry.category) || canSpawnEntry)) {
            int& spawnQty = spawnQuantities[entry.formID];
            if (spawnQty < 1) {
                spawnQty = 1;
            }

            const float inputWidth = 80.0f;
            const char* spawnLabel = L("NPCs", "sSpawnAtPlayer", "Spawn At Player");

            if (ImGui::MenuItem(spawnLabel)) {
                if (canSpawnEntry && callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    const auto quantity = static_cast<std::uint32_t>(spawnQty);
                    const std::string name = entry.name;
                    const auto spawnAction = callbacks.spawnEntry;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmSpawnTitle", "Confirm Spawn"),
                        std::string(L("General", "sConfirmSpawnMessage", "Spawn selected record at player?")) + "\n" + (name.empty() ? L("General", "sUnnamed", "<Unnamed>") : name),
                        [entry, formID, quantity, spawnAction]() {
                            if (spawnAction) {
                                spawnAction(entry, quantity);
                                return;
                            }
                            FormActions::SpawnAtPlayer(formID, quantity);
                        });
                } else {
                    if (callbacks.spawnEntry) {
                        callbacks.spawnEntry(entry, static_cast<std::uint32_t>(spawnQty));
                    } else {
                        FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(spawnQty));
                    }
                }
            }

            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputInt(L("General", "sQuantity", "Quantity"), &spawnQty, 1, 10);
            if (spawnQty < 1) {
                spawnQty = 1;
            }
        }

        if (IsEquippable(entry.category)) {
            if (ImGui::MenuItem(L("General", "sEquipItem", "Equip Item"))) {
                char command[64]{};
                std::snprintf(command, sizeof(command), "player.equipitem %08X", entry.formID);
                FormActions::ExecuteConsoleCommand(command);

                const bool isWeapon = entry.category == "WEAP" || entry.category == "Weapon";
                if (isWeapon && callbacks.equipWeaponAmmoCount > 0) {
                    const auto ammoFormID = FormActions::GetWeaponAmmoFormID(entry.formID);
                    if (ammoFormID != 0) {
                        FormActions::GiveToPlayer(ammoFormID, static_cast<std::uint32_t>(callbacks.equipWeaponAmmoCount));
                    }
                }
            }
        }

        if (IsSpellLike(entry.category)) {
            if (ImGui::MenuItem(L("General", "sAddSpellEffect", "Add Spell/Effect"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmAction", "Confirm Action"),
                        L("General", "sConfirmAddSpellEffect", "Add selected spell/effect to player?"),
                        [formID]() {
                            FormActions::AddSpellToPlayer(formID);
                        });
                } else {
                    FormActions::AddSpellToPlayer(entry.formID);
                }
            }
            if (ImGui::MenuItem(L("General", "sRemoveSpellEffect", "Remove Spell/Effect"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmAction", "Confirm Action"),
                        L("General", "sConfirmRemoveSpellEffect", "Remove selected spell/effect from player?"),
                        [formID]() {
                            FormActions::RemoveSpellFromPlayer(formID);
                        });
                } else {
                    FormActions::RemoveSpellFromPlayer(entry.formID);
                }
            }
        }

        if (IsPerk(entry.category)) {
            if (ImGui::MenuItem(L("General", "sAddPerk", "Add Perk"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmAction", "Confirm Action"),
                        L("General", "sConfirmAddPerk", "Add selected perk to player?"),
                        [formID]() {
                            FormActions::AddPerkToPlayer(formID);
                        });
                } else {
                    FormActions::AddPerkToPlayer(entry.formID);
                }
            }
            if (ImGui::MenuItem(L("General", "sRemovePerk", "Remove Perk"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmAction", "Confirm Action"),
                        L("General", "sConfirmRemovePerk", "Remove selected perk from player?"),
                        [formID]() {
                            FormActions::RemovePerkFromPlayer(formID);
                        });
                } else {
                    FormActions::RemovePerkFromPlayer(entry.formID);
                }
            }
        }

        if (IsQuest(entry.category)) {
            if (ImGui::MenuItem(L("General", "sStartQuest", "Start Quest"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmQuestTitle", "Confirm Quest Action"),
                        L("General", "sConfirmStartQuest", "Start selected quest?"),
                        [formID]() {
                            char cmd[64]{};
                            std::snprintf(cmd, sizeof(cmd), "startquest %08X", formID);
                            FormActions::ExecuteConsoleCommand(cmd);
                        });
                } else {
                    char cmd[64]{};
                    std::snprintf(cmd, sizeof(cmd), "startquest %08X", entry.formID);
                    FormActions::ExecuteConsoleCommand(cmd);
                }
            }
            if (ImGui::MenuItem(L("General", "sCompleteQuest", "Complete Quest"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmQuestTitle", "Confirm Quest Action"),
                        L("General", "sConfirmCompleteQuest", "Complete selected quest?"),
                        [formID]() {
                            char cmd[64]{};
                            std::snprintf(cmd, sizeof(cmd), "completequest %08X", formID);
                            FormActions::ExecuteConsoleCommand(cmd);
                        });
                } else {
                    char cmd[64]{};
                    std::snprintf(cmd, sizeof(cmd), "completequest %08X", entry.formID);
                    FormActions::ExecuteConsoleCommand(cmd);
                }
            }
        }

        if (IsWeather(entry.category)) {
            if (ImGui::MenuItem(L("General", "sSetWeather", "Set Weather"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmWeatherTitle", "Confirm Weather Change"),
                        L("General", "sConfirmWeather", "Set current weather to selected weather record?"),
                        [formID]() {
                            char cmd[64]{};
                            std::snprintf(cmd, sizeof(cmd), "fw %08X", formID);
                            FormActions::ExecuteConsoleCommand(cmd);
                        });
                } else {
                    char cmd[64]{};
                    std::snprintf(cmd, sizeof(cmd), "fw %08X", entry.formID);
                    FormActions::ExecuteConsoleCommand(cmd);
                }
            }
        }

        if (IsSound(entry.category) && editorID) {
            if (ImGui::MenuItem(L("General", "sPlaySound", "Play Sound"))) {
                std::string command = std::string("playsound ") + editorID;
                FormActions::ExecuteConsoleCommand(command);
            }
        }

        if (IsGlobal(entry.category) && callbacks.openGlobalValuePopup) {
            if (ImGui::MenuItem(L("General", "sSetGlobal", "Set Global"))) {
                callbacks.openGlobalValuePopup(entry.formID);
            }
        }

        if (IsOutfit(entry.category)) {
            if (ImGui::MenuItem(L("General", "sAddOutfitItems", "Add Outfit Items"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmAction", "Confirm Action"),
                        L("General", "sConfirmAddOutfitItems", "Add all items from selected outfit to player?"),
                        [formID]() {
                            FormActions::AddOutfitItemsToPlayer(formID);
                        });
                } else {
                    FormActions::AddOutfitItemsToPlayer(entry.formID);
                }
            }
        }

        if (IsConstructible(entry.category)) {
            if (ImGui::MenuItem(L("General", "sAddCraftedItem", "Add Crafted Item"))) {
                if (callbacks.requestActionConfirmation) {
                    const auto formID = entry.formID;
                    callbacks.requestActionConfirmation(
                        L("General", "sConfirmAction", "Confirm Action"),
                        L("General", "sConfirmAddCraftedItem", "Add crafted output of selected recipe to player?"),
                        [formID]() {
                            FormActions::AddConstructedItemToPlayer(formID);
                        });
                } else {
                    FormActions::AddConstructedItemToPlayer(entry.formID);
                }
            }
        }

        if (CanTeleport(entry.category)) {
            const bool canUseCoc = editorID != nullptr;
            if (canUseCoc) {
                if (ImGui::MenuItem(L("General", "sTeleportCOC", "Teleport (COC)"))) {
                    if (callbacks.requestActionConfirmation) {
                        const auto editorIDCopy = std::string(editorID);
                        callbacks.requestActionConfirmation(
                            L("General", "sConfirmTeleportTitle", "Confirm Teleport"),
                            L("General", "sConfirmTeleport", "Teleport to selected destination?"),
                            [editorIDCopy]() {
                                std::string command = std::string("coc ") + editorIDCopy;
                                FormActions::ExecuteConsoleCommand(command);
                            });
                    } else {
                        std::string command = std::string("coc ") + editorID;
                        FormActions::ExecuteConsoleCommand(command);
                    }
                }
            } else {
                ImGui::BeginDisabled(true);
                ImGui::MenuItem(L("General", "sTeleportCOC", "Teleport (COC)"));
                ImGui::EndDisabled();
            }
        }

        if (!callbacks.hideCopyAndFavoriteActions) {
            ImGui::Separator();

            if (ImGui::MenuItem(L("General", "sCopyFormID", "Copy FormID"))) {
                FormActions::CopyFormID(entry.formID);
            }

            if (ImGui::MenuItem(L("General", "sCopyRecordSource", "Copy Record Source"))) {
                ImGui::SetClipboardText(entry.sourcePlugin.c_str());
            }

            if (ImGui::MenuItem(L("General", "sCopyName", "Copy Name"))) {
                ImGui::SetClipboardText(displayName);
            }

            if (editorID) {
                if (ImGui::MenuItem(L("General", "sCopyEditorID", "Copy EditorID"))) {
                    ImGui::SetClipboardText(editorID);
                }
            }

            ImGui::Separator();

            if (callbacks.favorites) {
                const bool isFavorite = callbacks.favorites->contains(entry.formID);
                if (ImGui::MenuItem(isFavorite ? L("General", "sRemoveFavorite", "Remove Favorite") : L("General", "sAddFavorite", "Add Favorite"))) {
                    if (isFavorite) {
                        callbacks.favorites->erase(entry.formID);
                    } else {
                        callbacks.favorites->insert(entry.formID);
                    }
                }
            }
        }
    }
}
