#include "pch.h"
#include "Game/HostLifecycle.h"
#include <RE/M/Main.h>

namespace ESPExplorerAE
{
    bool IsHostQuitRequested()
    {
        const auto* main = RE::Main::GetSingleton();
        return main && main->quitGame;
    }
}
