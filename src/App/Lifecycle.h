#pragma once

#include "Core/Shutdown.h"

namespace ESPExplorerAE
{
    class Lifecycle
    {
    public:
        static ShutdownCoordinator& Shutdown();
    };
}
