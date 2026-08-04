#include "pch.h"
#include "Game/PlayerStatusReader.h"
#include <RE/A/ActorValue.h>
#include <RE/P/PlayerCharacter.h>
#include <cmath>

namespace ESPExplorerAE
{
    PlayerStatus ReadPlayerStatus(std::uint64_t session)
    {
        PlayerStatus result{ .session = session };
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* values = RE::ActorValue::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (!player || !player->GetParentCell() || !values || !values->health || !values->actionPoints || !ui || ui->GetMenuOpen<RE::MainMenu>()) return result;
        result.level = player->GetLevel();
        result.caps = player->GetGoldAmount();
        result.health = player->GetActorValue(*values->health);
        result.actionPoints = player->GetActorValue(*values->actionPoints);
        result.ready = std::isfinite(result.health) && std::isfinite(result.actionPoints);
        return result;
    }
}
