#pragma once

#include "Core/Theme.h"
#include "Core/LanguageSnapshot.h"
#include <memory>

namespace ESPExplorerAE
{
    struct SettingsResources
    {
        std::shared_ptr<const std::vector<ThemePreset>> themes;
        std::shared_ptr<const std::vector<LanguageDefinition>> languages;
        PipboyColor pipboyColor;
        std::string toggleKeyName;
        std::string gameVersion;
        std::string modVersion;
        bool gamepadConnected{};
    };

    enum class SettingsPage { NexusMods, GitHub, BugReport, Donations };
}
