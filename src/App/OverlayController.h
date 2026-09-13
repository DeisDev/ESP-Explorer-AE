#pragma once

#include "Core/OverlayPolicy.h"

namespace ESPExplorerAE
{
    class OverlayController
    {
    public:
        static void SetVisible(bool visible);
        static void Toggle();
        static void SetFocused(bool focused);
        static void SetModal(bool modal);
        static void SetRendererState(bool ready, bool keyboardDialog);
        static void Configure(OverlaySettings settings);
        static void SetWorldState(bool blocked, bool ready, bool powerArmor, bool mainMenu = false);
        static OverlayFacts Facts();
        static OverlayDecision Decision();
    };
}
