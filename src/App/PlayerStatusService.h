#pragma once
#include "Core/PlayerStatus.h"

namespace ESPExplorerAE
{
    class PlayerStatusService
    {
    public:
        static PlayerStatus Request(std::uint64_t session);
        static void ResetSession();
        static void Pump(std::uint64_t session, bool ready);
    };
}
