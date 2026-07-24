#pragma once

#include "Core/Theme.h"

namespace ESPExplorerAE
{
    class SettingsReader
    {
    public:
        static void Pump(); // F4SE game task owns all engine setting reads.
        static PipboyColor Color(); // Detached, synchronized render-thread read.
    };
}
