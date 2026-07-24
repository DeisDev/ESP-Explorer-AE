#pragma once
#include "Core/PlayerStatus.h"

namespace ESPExplorerAE
{
    // Game task only. Returned status contains no engine pointers or views.
    PlayerStatus ReadPlayerStatus(std::uint64_t session);
}
