#include "pch.h"
#include "Game/OverlayEffects.h"
#include "App/OverlayController.h"
#include "Core/OverlayPolicy.h"

#include <RE/C/ControlMap.h>
#include <RE/C/Console.h>
#include <RE/M/Main.h>
#include <atomic>

namespace ESPExplorerAE
{
    namespace
    {
        TemporaryOverride<bool> ignoreInput(true);
        TemporaryOverride<bool> pause(true);
        TemporaryOverride<bool> hideHUD(false);
        TemporaryOverride<bool> godMode(true);
        bool restoringHUD{};
        std::atomic<bool> observedGodMode{};

        bool BlockingMenu(RE::UI* ui)
        {
            if (!ui) return true;
            static const std::array<RE::BSFixedString, 12> names{
                "BarterMenu", "ContainerMenu", "DialogueMenu", "LevelUpMenu", "LockpickingMenu",
                "LooksMenu", "PipboyMenu", "PipboyWorkshopMenu", "SleepWaitMenu", "TerminalMenu", "WorkshopMenu", "LoadingMenu"
            };
            return std::ranges::any_of(names, [&](const auto& name) { return ui->GetMenuOpen(name); });
        }

        void Apply(const OverlayDecision& decision)
        {
            if (auto* controls = RE::ControlMap::GetSingleton()) {
                ignoreInput.Update(decision.capture, controls->ignoreKeyboardMouse, [&](bool value) {
                    controls->ignoreKeyboardMouse = value;
                    return true;
                });
            }
            if (auto* main = RE::Main::GetSingleton()) {
                pause.Update(decision.pause, main->freezeTime, [&](bool value) {
                    main->freezeTime = value;
                    return true;
                });
            }
            if (auto* ui = RE::UI::GetSingleton()) {
                if (const auto hud = ui->GetMenu<RE::HUDMenu>()) {
                    const bool shown = hud->hudShowMenuState.get() == RE::HUDMenu::ShowMenuState::kShown;
                    hideHUD.Update(decision.hideHUD && !restoringHUD, shown, [&](bool value) {
                        auto* queue = RE::UIMessageQueue::GetSingleton();
                        if (!queue) return false;
                        queue->AddMessage(RE::HUDMenu::MENU_NAME, value ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide);
                        return true;
                    });
                    if (!hideHUD.Engaged()) restoringHUD = false;
                }
            }
            if (auto* player = RE::PlayerCharacter::GetSingleton()) {
                godMode.Update(decision.godMode, player->IsGodMode(), [&](bool value) {
                    if (player->IsGodMode() != value) RE::Console::ExecuteCommand("tgm");
                    return true;
                });
                observedGodMode = player->IsGodMode();
            } else {
                observedGodMode = false;
            }
        }
    }

    void OverlayEffects::Update(bool worldReady)
    {
        auto* ui = RE::UI::GetSingleton();
        static const RE::BSFixedString powerArmorHUD("PowerArmorHUDMenu");
        OverlayController::SetWorldState(BlockingMenu(ui), worldReady, ui && ui->GetMenuOpen(powerArmorHUD));
        Apply(OverlayController::Decision());
    }

    void OverlayEffects::EndSession()
    {
        // A queued HUD hide may complete after the lifecycle notification. Keep
        // its restoration obligation until observed instead of silently losing
        // it. No new HUD override is admitted until it settles.
        restoringHUD = hideHUD.Engaged();
        if (ignoreInput.Engaged() || pause.Engaged() || hideHUD.Engaged() || godMode.Engaged()) Apply({});
        ignoreInput.Forget();
        pause.Forget();
        godMode.Forget();
        observedGodMode = false;
        OverlayController::SetWorldState(true, false, false);
    }

    bool OverlayEffects::RestoreForShutdown()
    {
        restoringHUD = hideHUD.Engaged();
        Apply({});
        OverlayController::SetWorldState(true, false, false);
        return !ignoreInput.RequiresRestoration() && !pause.RequiresRestoration() && !hideHUD.RequiresRestoration() && !godMode.RequiresRestoration();
    }

    bool OverlayEffects::GodModeEnabled() { return observedGodMode; }
}
