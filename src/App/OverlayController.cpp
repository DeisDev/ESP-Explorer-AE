#include "App/OverlayController.h"

#include <mutex>

namespace ESPExplorerAE
{
    namespace
    {
        std::mutex mutex;
        OverlayFacts facts;
        OverlaySettings settings;
    }

    void OverlayController::SetVisible(bool visible) { std::lock_guard lock(mutex); facts.visible = visible; }
    void OverlayController::Toggle() { std::lock_guard lock(mutex); facts.visible = !facts.visible; }
    void OverlayController::SetFocused(bool focused) { std::lock_guard lock(mutex); facts.focused = focused; }
    void OverlayController::SetModal(bool modal) { std::lock_guard lock(mutex); facts.modal = modal; }
    void OverlayController::SetRendererState(bool ready, bool keyboardDialog)
    {
        std::lock_guard lock(mutex);
        facts.rendererReady = ready;
        facts.keyboardDialog = keyboardDialog;
    }
    void OverlayController::Configure(OverlaySettings value) { std::lock_guard lock(mutex); settings = value; }
    void OverlayController::SetWorldState(bool blocked, bool ready, bool powerArmor, bool mainMenu)
    {
        std::lock_guard lock(mutex);
        facts.blocked = blocked;
        facts.worldReady = ready;
        facts.powerArmorHUD = powerArmor;
        facts.mainMenu = mainMenu;
    }
    OverlayFacts OverlayController::Facts() { std::lock_guard lock(mutex); return facts; }
    OverlayDecision OverlayController::Decision() { std::lock_guard lock(mutex); return DecideOverlay(facts, settings); }
}
