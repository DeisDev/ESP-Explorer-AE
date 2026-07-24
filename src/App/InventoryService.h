#pragma once

#include "Core/InventorySnapshot.h"

namespace ESPExplorerAE
{
    class InventoryService
    {
    public:
        static std::shared_ptr<const InventorySnapshot> Request(std::uint64_t session, bool force = false);
        static void ResetSession();
        static void Invalidate();
        static void Pump(std::uint64_t session, bool ready, bool pendingActions);
    };
}
