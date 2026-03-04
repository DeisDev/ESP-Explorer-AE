#include "GUI/Tabs/PlayerTab.h"

#include "GUI/Widgets/FormActions.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void PlayerTab::Draw(
        const FormCache& cache,
        bool& playerGodModeEnabled,
        bool& playerNoClipEnabled,
        int& playerCurrentWeaponAmmoAmount,
        int& playerAllAmmoAmount,
        int& playerPerkPointsAmount,
        int& playerLevelAmount,
        const OpenItemGrantPopupFn& openItemGrantPopup,
        const LocalizeFn& localize)
    {
        auto wrappedSameLine = [](const char* nextLabel) {
            const auto& style = ImGui::GetStyle();
            const float nextWidth = ImGui::CalcTextSize(nextLabel).x + style.FramePadding.x * 2.0f;
            const float needed = style.ItemSpacing.x + nextWidth;
            if (ImGui::GetContentRegionAvail().x >= needed) {
                ImGui::SameLine();
            }
        };

        ImGui::SeparatorText(localize("Player", "sQuickActions", "Quick Actions"));

        if (ImGui::Button(localize("Player", "sRefillHealth", "Refill Health"))) {
            FormActions::ExecuteConsoleCommand("player.resethealth");
        }

        const char* godModeLabel = playerGodModeEnabled ? localize("Player", "sGodModeOff", "Godmode: ON") : localize("Player", "sGodModeOn", "Godmode: OFF");
        wrappedSameLine(godModeLabel);
        if (ImGui::Button(godModeLabel)) {
            FormActions::ExecuteConsoleCommand("tgm");
            playerGodModeEnabled = !playerGodModeEnabled;
        }

        const char* noClipLabel = playerNoClipEnabled ? localize("Player", "sNoClipOff", "Noclip: ON") : localize("Player", "sNoClipOn", "Noclip: OFF");
        wrappedSameLine(noClipLabel);
        if (ImGui::Button(noClipLabel)) {
            FormActions::ExecuteConsoleCommand("tcl");
            playerNoClipEnabled = !playerNoClipEnabled;
        }

        ImGui::SeparatorText(localize("Player", "sCharacterSection", "Character"));

        const float inputWidth = (std::max)(120.0f, ImGui::GetContentRegionAvail().x * 0.2f);

        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputInt(localize("Player", "sSetLevel", "Set Level"), &playerLevelAmount, 1, 10);
        if (playerLevelAmount < 1) {
            playerLevelAmount = 1;
        }
        if (playerLevelAmount > 65535) {
            playerLevelAmount = 65535;
        }
        const char* setLevelLabel = localize("Player", "sApplyLevel", "Apply Level");
        wrappedSameLine(setLevelLabel);
        if (ImGui::Button(setLevelLabel)) {
            char command[64]{};
            std::snprintf(command, sizeof(command), "player.setlevel %d", playerLevelAmount);
            FormActions::ExecuteConsoleCommand(command);
        }

        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputInt(localize("Player", "sAddPerkPoints", "Perk Points"), &playerPerkPointsAmount, 1, 5);
        if (playerPerkPointsAmount < 1) {
            playerPerkPointsAmount = 1;
        }
        if (playerPerkPointsAmount > 999) {
            playerPerkPointsAmount = 999;
        }
        const char* addPerkPointsLabel = localize("Player", "sAddPerkPointsBtn", "Add Perk Points");
        wrappedSameLine(addPerkPointsLabel);
        if (ImGui::Button(addPerkPointsLabel)) {
            char command[128]{};
            std::snprintf(command, sizeof(command), "cgf \"Game.AddPerkPoints\" %d", playerPerkPointsAmount);
            FormActions::ExecuteConsoleCommand(command);
        }

        ImGui::SeparatorText(localize("Player", "sAmmunitionSection", "Ammunition"));

        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputInt(localize("Player", "sCurrentWeaponAmmo", "Current Weapon Ammo"), &playerCurrentWeaponAmmoAmount, 10, 100);
        if (playerCurrentWeaponAmmoAmount < 1) {
            playerCurrentWeaponAmmoAmount = 1;
        }
        const char* addCurrentAmmoLabel = localize("Player", "sAddCurrentAmmo", "Add Ammo For Held Weapon");
        wrappedSameLine(addCurrentAmmoLabel);
        if (ImGui::Button(addCurrentAmmoLabel)) {
            FormActions::AddAmmoForCurrentWeapon(static_cast<std::uint32_t>(playerCurrentWeaponAmmoAmount));
        }

        ImGui::SetNextItemWidth(inputWidth);
        ImGui::InputInt(localize("Player", "sAllAmmoCount", "All Ammo Count"), &playerAllAmmoAmount, 10, 100);
        if (playerAllAmmoAmount < 1) {
            playerAllAmmoAmount = 1;
        }
        const char* addAllAmmoLabel = localize("Player", "sAddAllAmmo", "Add All Ammo Types");
        wrappedSameLine(addAllAmmoLabel);
        if (ImGui::Button(addAllAmmoLabel)) {
            for (const auto& ammo : cache.ammo) {
                FormActions::GiveToPlayer(ammo.formID, static_cast<std::uint32_t>(playerAllAmmoAmount));
            }
        }

        ImGui::SeparatorText(localize("Player", "sQuickItemsSection", "Quick Items"));

        if (ImGui::Button(localize("Player", "sAddStimpak", "Add Stimpaks"))) {
            FormEntry entry{};
            entry.formID = FormActions::kStimpakFormID;
            entry.name = localize("Player", "sItemStimpak", "Stimpak");
            entry.category = "Aid";
            openItemGrantPopup(entry);
        }

        const char* lockpickLabel = localize("Player", "sAddLockpick", "Add Lockpicks");
        wrappedSameLine(lockpickLabel);
        if (ImGui::Button(lockpickLabel)) {
            FormEntry entry{};
            entry.formID = FormActions::kLockpickFormID;
            entry.name = localize("Player", "sItemLockpick", "Lockpick");
            entry.category = "Misc";
            openItemGrantPopup(entry);
        }
    }
}
