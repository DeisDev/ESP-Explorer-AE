#pragma once

#include "GUI/BrowserState.h"
#include "Core/PlayerStatus.h"

namespace ESPExplorerAE
{
    struct PlayerWorldState
    {
        int level{ 1 };
        int perkPoints{ 1 };
        float gameHour{ 12.0f };
        std::uint32_t weather{};
        std::array<char, 128> weatherSearch{};
        ActionAdmission admission{ ActionAdmission::Accepted };
    };
    void DrawPlayerWorld(PlayerWorldState& state, const BrowserView& view, PlayerStatus player, bool godMode, BrowserRequests& requests);
}
