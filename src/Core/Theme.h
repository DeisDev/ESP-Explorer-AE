#pragma once

#include "Core/Settings.h"
#include <algorithm>
#include <cmath>
#include <span>

namespace ESPExplorerAE
{
    struct PipboyColor
    {
        float r{};
        float g{};
        float b{};
        bool valid{};
        friend bool operator==(const PipboyColor&, const PipboyColor&) = default;
    };
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

    inline const ThemePreset DefaultTheme{
        .id = "modern-charcoal",
        .name = "Modern Charcoal",
        .nameKey = "sPresetModernCharcoal",
        .accentR = 0.38f,
        .accentG = 0.78f,
        .accentB = 0.71f,
        .accentA = 1.0f,
        .windowR = 0.055f,
        .windowG = 0.063f,
        .windowB = 0.078f,
        .windowA = 0.96f,
        .panelR = 0.080f,
        .panelG = 0.090f,
        .panelB = 0.110f,
        .panelA = 0.94f,
        .builtIn = true
    };

    inline bool NearlyEqualThemeColor(float left, float right)
    {
        return std::fabs(left - right) <= 0.01f;
    }

    inline bool SameThemeColors(const Settings& settings, const ThemePreset& theme)
    {
        return NearlyEqualThemeColor(settings.themeAccentR, theme.accentR) &&
            NearlyEqualThemeColor(settings.themeAccentG, theme.accentG) &&
            NearlyEqualThemeColor(settings.themeAccentB, theme.accentB) &&
            NearlyEqualThemeColor(settings.themeAccentA, theme.accentA) &&
            NearlyEqualThemeColor(settings.themeWindowR, theme.windowR) &&
            NearlyEqualThemeColor(settings.themeWindowG, theme.windowG) &&
            NearlyEqualThemeColor(settings.themeWindowB, theme.windowB) &&
            NearlyEqualThemeColor(settings.themeWindowA, theme.windowA) &&
            NearlyEqualThemeColor(settings.themePanelR, theme.panelR) &&
            NearlyEqualThemeColor(settings.themePanelG, theme.panelG) &&
            NearlyEqualThemeColor(settings.themePanelB, theme.panelB) &&
            NearlyEqualThemeColor(settings.themePanelA, theme.panelA);
    }

    inline const ThemePreset* FindTheme(std::span<const ThemePreset> themes, std::string_view id)
    {
        const auto found = std::ranges::find(themes, id, &ThemePreset::id);
        return found == themes.end() ? nullptr : &*found;
    }

    inline const ThemePreset* FindMatchingTheme(std::span<const ThemePreset> themes, const Settings& settings)
    {
        const auto found = std::ranges::find_if(themes, [&](const auto& theme) { return SameThemeColors(settings, theme); });
        return found == themes.end() ? nullptr : &*found;
    }

    inline bool ApplyPipboyColor(Settings& settings, PipboyColor color)
    {
        if (!color.valid || !std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) return false;
        const auto max = (std::max)({ color.r, color.g, color.b, 0.01f });
        const auto r = color.r / max, g = color.g / max, b = color.b / max;
        const auto before = settings;
        settings.themeAccentR = (std::clamp)(r * 0.94f, 0.0f, 1.0f);
        settings.themeAccentG = (std::clamp)(g * 0.94f, 0.0f, 1.0f);
        settings.themeAccentB = (std::clamp)(b * 0.94f, 0.0f, 1.0f);
        settings.themeAccentA = 1.0f;
        settings.themeWindowR = (std::clamp)(r * 0.06f, 0.0f, 1.0f);
        settings.themeWindowG = (std::clamp)(g * 0.06f, 0.0f, 1.0f);
        settings.themeWindowB = (std::clamp)(b * 0.06f, 0.0f, 1.0f);
        settings.themeWindowA = 0.96f;
        settings.themePanelR = (std::clamp)(r * 0.11f, 0.0f, 1.0f);
        settings.themePanelG = (std::clamp)(g * 0.11f, 0.0f, 1.0f);
        settings.themePanelB = (std::clamp)(b * 0.11f, 0.0f, 1.0f);
        settings.themePanelA = 0.94f;
        settings.themePresetId.clear();
        return settings != before;
    }

    inline void ApplyTheme(Settings& settings, const ThemePreset& theme)
    {
        settings.themeAccentR = theme.accentR;
        settings.themeAccentG = theme.accentG;
        settings.themeAccentB = theme.accentB;
        settings.themeAccentA = theme.accentA;
        settings.themeWindowR = theme.windowR;
        settings.themeWindowG = theme.windowG;
        settings.themeWindowB = theme.windowB;
        settings.themeWindowA = theme.windowA;
        settings.themePanelR = theme.panelR;
        settings.themePanelG = theme.panelG;
        settings.themePanelB = theme.panelB;
        settings.themePanelA = theme.panelA;
        settings.themePresetId = theme.id;
    }
}
