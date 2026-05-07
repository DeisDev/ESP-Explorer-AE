#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    struct Settings;

    struct ThemePreset
    {
        std::string id;
        std::string name;
        std::string nameKey;
        float accentR, accentG, accentB, accentA;
        float windowR, windowG, windowB, windowA;
        float panelR, panelG, panelB, panelA;
        bool builtIn{ false };
    };

    class ThemeManager
    {
    public:
        static std::filesystem::path ResolveThemesDirectory();
        static void ReloadAvailableThemes();
        static const std::vector<ThemePreset>& GetAvailableThemes();
        static const ThemePreset& GetDefaultTheme();
        static const ThemePreset* FindThemeById(std::string_view id);
        static const ThemePreset* FindMatchingTheme(const Settings& settings);
        static void ApplyTheme(Settings& settings, const ThemePreset& theme);
    };
}
