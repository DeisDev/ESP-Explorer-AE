#pragma once

#include "Core/CatalogSnapshot.h"
#include <memory>

namespace ESPExplorerAE
{
    // Coalesces lifecycle/refresh requests and publishes owning snapshots.
    // Only Pump enters the game reader, on the application game task.
    class CatalogService
    {
    public:
        static void RequestRefresh();
        static void SetAvailable(bool available, bool waitForWorld = false);
        static void Pump(bool worldReady);
        static std::shared_ptr<const CatalogSnapshot> Read();
    };
}
