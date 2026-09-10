#pragma once

#include "Core/Settings.h"
#include "Core/FontSizes.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace ESPExplorerAE
{
    inline constexpr std::array<std::string_view, 9> MainTabIDs{
        "Plugin Browser", "Inventory", "Item Browser", "NPC Browser", "Cell Browser", "Object Browser", "Spells & Perks", "Settings", "Logs"
    };

    inline bool ValidLanguageCode(std::string_view code)
    {
        if (code.empty() || code.size() > 251 || code == "." || code == ".." || code.back() == '.' || code.back() == ' ') return false;
        for (const unsigned char ch : code) if (ch < 32 || ch == 127 || std::string_view("<>:\"/\\|?*").find(static_cast<char>(ch)) != std::string_view::npos) return false;
        std::string stem(code.substr(0, code.find('.')));
        for (auto& ch : stem) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") return false;
        return !(stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) && stem[3] >= '1' && stem[3] <= '9');
    }

    inline MultiCopyFormat ValidatedCopyFormat(long value)
    {
        return value >= static_cast<long>(MultiCopyFormat::Lines) && value <= static_cast<long>(MultiCopyFormat::QuotedCommaSeparated) ?
            static_cast<MultiCopyFormat>(value) : MultiCopyFormat::Lines;
    }

    inline std::vector<std::string> ValidateSettings(Settings& value)
    {
        const Settings defaults;
        std::vector<std::string> adjusted;
        const auto bounded = [&](float& field, float fallback, float low, float high, const char* name) {
            const auto next = std::isfinite(field) ? (std::clamp)(field, low, high) : fallback;
            if (field != next) { field = next; adjusted.emplace_back(name); }
        };
        const auto position = [&](float& field, float fallback, const char* name) {
            if (!std::isfinite(field)) { field = fallback; adjusted.emplace_back(name); }
        };
        if (!ValidLanguageCode(value.language)) { value.language = defaults.language; adjusted.emplace_back("General.sLanguage"); }
        if (!value.toggleKey || value.toggleKey > 0xFE || value.toggleKey == 0x09) {
            value.toggleKey = defaults.toggleKey;
            adjusted.emplace_back("General.iToggleKey");
        }
        bounded(value.fontSize, defaults.fontSize, FontSizes.front(), FontSizes.back(), "UI.fFontSize");
        bounded(value.windowAlpha, defaults.windowAlpha, 0.5f, 1.0f, "UI.fWindowAlpha");
        position(value.windowX, defaults.windowX, "UI.fWindowX");
        position(value.windowY, defaults.windowY, "UI.fWindowY");
        position(value.actionHistoryWindowX, defaults.actionHistoryWindowX, "UI.fActionHistoryWindowX");
        position(value.actionHistoryWindowY, defaults.actionHistoryWindowY, "UI.fActionHistoryWindowY");
        bounded(value.windowW, defaults.windowW, 1.0f, 4096.0f, "UI.fWindowW");
        bounded(value.windowH, defaults.windowH, 1.0f, 4096.0f, "UI.fWindowH");
        bounded(value.themeAccentR, defaults.themeAccentR, 0, 1, "Theme.fAccentR");
        bounded(value.themeAccentG, defaults.themeAccentG, 0, 1, "Theme.fAccentG");
        bounded(value.themeAccentB, defaults.themeAccentB, 0, 1, "Theme.fAccentB");
        bounded(value.themeAccentA, defaults.themeAccentA, 0, 1, "Theme.fAccentA");
        bounded(value.themeWindowR, defaults.themeWindowR, 0, 1, "Theme.fWindowR");
        bounded(value.themeWindowG, defaults.themeWindowG, 0, 1, "Theme.fWindowG");
        bounded(value.themeWindowB, defaults.themeWindowB, 0, 1, "Theme.fWindowB");
        bounded(value.themeWindowA, defaults.themeWindowA, 0, 1, "Theme.fWindowA");
        bounded(value.themePanelR, defaults.themePanelR, 0, 1, "Theme.fPanelR");
        bounded(value.themePanelG, defaults.themePanelG, 0, 1, "Theme.fPanelG");
        bounded(value.themePanelB, defaults.themePanelB, 0, 1, "Theme.fPanelB");
        bounded(value.themePanelA, defaults.themePanelA, 0, 1, "Theme.fPanelA");
        const auto recent = (std::clamp)(value.recentRecordsLimit, 5, 100);
        if (recent != value.recentRecordsLimit) { value.recentRecordsLimit = recent; adjusted.emplace_back("UI.iRecentRecordsLimit"); }
        const auto ammo = (std::clamp)(value.defaultAmmoQuantity, 0, 50000);
        if (ammo != value.defaultAmmoQuantity) { value.defaultAmmoQuantity = ammo; adjusted.emplace_back("General.iDefaultAmmoQuantity"); }
        const auto copy = ValidatedCopyFormat(static_cast<long>(value.multiCopyFormat));
        if (copy != value.multiCopyFormat) { value.multiCopyFormat = copy; adjusted.emplace_back("UI.iMultiCopyFormat"); }
        const auto tab = [&](std::string& field, const std::string& fallback, bool allowLast, const char* name) {
            if (field == "Player") { field = "Inventory"; adjusted.emplace_back(name); }
            else if (!(allowLast && field == "__last__") && std::ranges::find(MainTabIDs, field) == MainTabIDs.end()) { field = fallback; adjusted.emplace_back(name); }
        };
        tab(value.startupTab, defaults.startupTab, true, "UI.sStartupTab");
        tab(value.lastActiveTab, defaults.lastActiveTab, false, "UI.sLastActiveTab");
        return adjusted;
    }

    inline void ResetVisualSettings(Settings& settings)
    {
        const Settings defaults;
        settings.rememberWindowPos = defaults.rememberWindowPos;
        settings.fontSize = defaults.fontSize;
        settings.windowAlpha = defaults.windowAlpha;
        settings.themeAccentR = defaults.themeAccentR;
        settings.themeAccentG = defaults.themeAccentG;
        settings.themeAccentB = defaults.themeAccentB;
        settings.themeAccentA = defaults.themeAccentA;
        settings.themeWindowR = defaults.themeWindowR;
        settings.themeWindowG = defaults.themeWindowG;
        settings.themeWindowB = defaults.themeWindowB;
        settings.themeWindowA = defaults.themeWindowA;
        settings.themePanelR = defaults.themePanelR;
        settings.themePanelG = defaults.themePanelG;
        settings.themePanelB = defaults.themePanelB;
        settings.themePanelA = defaults.themePanelA;
        settings.syncPipboyColor = defaults.syncPipboyColor;
        settings.themePresetId = defaults.themePresetId;
    }
}
