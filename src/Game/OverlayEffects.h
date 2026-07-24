#pragma once

namespace ESPExplorerAE
{
    class OverlayEffects
    {
    public:
        static void Update(bool worldReady);
        static void EndSession();
        static bool RestoreForShutdown();
        static bool GodModeEnabled();
    };
}
