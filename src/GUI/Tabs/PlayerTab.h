#pragma once

#include "Data/DataManager.h"

#include <functional>

namespace ESPExplorerAE
{
    class PlayerTab
    {
    public:
        using OpenItemGrantPopupFn = std::function<void(const FormEntry&)>;
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        static void Draw(
            const FormCache& cache,
            bool& playerGodModeEnabled,
            bool& playerNoClipEnabled,
            int& playerCurrentWeaponAmmoAmount,
            int& playerAllAmmoAmount,
            int& playerPerkPointsAmount,
            int& playerLevelAmount,
            const OpenItemGrantPopupFn& openItemGrantPopup,
            const LocalizeFn& localize);
    };
}
