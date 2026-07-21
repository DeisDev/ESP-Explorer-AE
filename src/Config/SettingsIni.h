#pragma once

#include "Core/Settings.h"

namespace ESPExplorerAE
{
    struct SettingsReadResult
    {
        Settings settings;
        bool legacyFavorites{};
        std::vector<std::string> adjusted;
        std::string error;
        explicit operator bool() const { return error.empty(); }
    };

    SettingsReadResult ReadSettingsIni(std::string_view bytes);
    bool WriteSettingsIni(const Settings& settings, std::string& bytes);
}
