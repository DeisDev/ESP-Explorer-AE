#pragma once

#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    class CellBrowserTab
    {
    public:
        static void Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests);
    };
}
