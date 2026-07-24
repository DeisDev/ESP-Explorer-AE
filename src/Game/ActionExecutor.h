#pragma once

#include "Core/Actions.h"
#include <functional>

namespace ESPExplorerAE
{
    class ActionExecutor
    {
    public:
        // Game task context; the application supplies the session checkpoint.
        static ActionOutcome Execute(const ActionRequest& request, const std::function<bool()>& isCurrent);
        static bool AreGameplayActionsAllowed(bool debugOverride = false);

    private:
        static std::uint32_t GetWeaponAmmoFormID(std::uint32_t weaponFormID);
        static bool IsPlayerGodModeEnabled();
    };
}
