#pragma once

#include "Core/SettingsResources.h"
#include <optional>

namespace ESPExplorerAE
{
    class SettingsService
    {
    public:
        static SettingsResources Read();
        static void Apply(std::optional<Settings> settings, bool reloadThemes);
        static void PumpRender();
        static void OpenPage(SettingsPage page);
    };
}
