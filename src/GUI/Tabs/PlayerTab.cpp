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
        const OpenItemGrantPopupFn& openItemGrantPopup,
        const LocalizeFn& localize)
    {
        if (ImGui::Button(localize("Player", "sRefillHealth", "Refill Health"))) {
            FormActions::ExecuteConsoleCommand("player.resethealth");
        }

        ImGui::SameLine();
        if (ImGui::Button(playerGodModeEnabled ? localize("Player", "sGodModeOff", "Godmode: ON") : localize("Player", "sGodModeOn", "Godmode: OFF"))) {
            FormActions::ExecuteConsoleCommand("tgm");
            playerGodModeEnabled = !playerGodModeEnabled;
        }

        ImGui::SameLine();
        if (ImGui::Button(playerNoClipEnabled ? localize("Player", "sNoClipOff", "Noclip: ON") : localize("Player", "sNoClipOn", "Noclip: OFF"))) {
            FormActions::ExecuteConsoleCommand("tcl");
            playerNoClipEnabled = !playerNoClipEnabled;
        }

        ImGui::Separator();

        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt(localize("Player", "sCurrentWeaponAmmo", "Current Weapon Ammo"), &playerCurrentWeaponAmmoAmount, 10, 100);
        if (playerCurrentWeaponAmmoAmount < 1) {
            playerCurrentWeaponAmmoAmount = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button(localize("Player", "sAddCurrentAmmo", "Add Ammo For Held Weapon"))) {
            FormActions::AddAmmoForCurrentWeapon(static_cast<std::uint32_t>(playerCurrentWeaponAmmoAmount));
        }

        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputInt(localize("Player", "sAllAmmoCount", "All Ammo Count"), &playerAllAmmoAmount, 10, 100);
        if (playerAllAmmoAmount < 1) {
            playerAllAmmoAmount = 1;
        }
        ImGui::SameLine();
        if (ImGui::Button(localize("Player", "sAddAllAmmo", "Add All Ammo Types"))) {
            for (const auto& ammo : cache.ammo) {
                FormActions::GiveToPlayer(ammo.formID, static_cast<std::uint32_t>(playerAllAmmoAmount));
            }
        }

        ImGui::Separator();

        if (ImGui::Button(localize("Player", "sAddStimpak", "Add Stimpaks"))) {
            FormEntry entry{};
            entry.formID = FormActions::kStimpakFormID;
            entry.name = localize("Player", "sItemStimpak", "Stimpak");
            entry.category = "Aid";
            openItemGrantPopup(entry);
        }

        ImGui::SameLine();
        if (ImGui::Button(localize("Player", "sAddLockpick", "Add Lockpicks"))) {
            FormEntry entry{};
            entry.formID = FormActions::kLockpickFormID;
            entry.name = localize("Player", "sItemLockpick", "Lockpick");
            entry.category = "Misc";
            openItemGrantPopup(entry);
        }
    }
}
