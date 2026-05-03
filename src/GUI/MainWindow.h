#pragma once

namespace ESPExplorerAE
{
    class MainWindow
    {
    public:
        static void Draw();
        static void ResetStateFromConfig();
        static void HandleMenuVisibilityChanged(bool visible);
    };
}
