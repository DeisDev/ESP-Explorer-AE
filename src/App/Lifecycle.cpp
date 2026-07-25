#include "App/Lifecycle.h"

namespace ESPExplorerAE
{
    ShutdownCoordinator& Lifecycle::Shutdown()
    {
        static ShutdownCoordinator shutdown;
        return shutdown;
    }
}
