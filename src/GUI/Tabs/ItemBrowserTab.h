#pragma once

#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    class ItemBrowserTab
    {
    public:
        static void Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests);
    };
}
