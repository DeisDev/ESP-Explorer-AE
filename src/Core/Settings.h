#pragma once

#include "Core/CopyFormat.h"
#include "Core/FavoriteDocument.h"
#include <cstdint>

namespace ESPExplorerAE
{
    struct Settings
    {
        std::string language{ "en" };
        std::uint32_t toggleKey{ 0x2D };
        bool showOnStartup{ false };
        bool pauseGameWhenMenuOpen{ false };
        bool hidePlayerHUDWhenMenuOpen{ false };
        bool godModeWhenMenuOpen{ false };
        bool debugLogging{ true };
        bool profilePerformance{};
        bool showLogsTab{ true };
        bool hideNonPlayable{ true };
        bool hideDeleted{ true };
        bool hideNoName{ true };
        bool listShowPlayable{ true };
        bool listShowNonPlayable{ false };
        bool listShowNamed{ true };
        bool listShowUnnamed{ false };
        bool listShowDeleted{ true };
        std::string advancedRecordFilters{};
        std::string hiddenPlugins{};
        bool pluginGlobalSearchMode{ false };
        bool pluginShowUnknownCategories{ false };
        bool autoFocusSearchBars{ true };
        bool showPlayerStatsInStatus{ false };
        bool showMenuResolutionInStatus{ false };
        MultiCopyFormat multiCopyFormat{ MultiCopyFormat::Lines };
        bool allowGameplayActionsInMainMenu{ false };
        bool componentSubstitution{ true };
        bool pluginAdvancedDetailsView{ false };
        int recentRecordsLimit{ 25 };
        bool enableGamepadNav{ true };
        bool showFPSInStatus{ true };
        bool rememberWindowPos{ true };
        std::string startupTab{ "__last__" };
        bool firstRunHelpDismissed{ false };
        float actionHistoryWindowX{ 200.0f };
        float actionHistoryWindowY{ 160.0f };
        float fontSize{ 20.0f };
        float windowAlpha{ 0.95f };
        float windowX{ 100.0f };
        float windowY{ 100.0f };
        float windowW{ 1440.0f };
        float windowH{ 810.0f };
        float themeAccentR{ 0.38f };
        float themeAccentG{ 0.78f };
        float themeAccentB{ 0.71f };
        float themeAccentA{ 1.0f };
        float themeWindowR{ 0.055f };
        float themeWindowG{ 0.063f };
        float themeWindowB{ 0.078f };
        float themeWindowA{ 0.96f };
        float themePanelR{ 0.080f };
        float themePanelG{ 0.090f };
        float themePanelB{ 0.110f };
        float themePanelA{ 0.94f };
        bool syncPipboyColor{ false };
        std::string themePresetId{ "modern-charcoal" };
        std::string lastActiveTab{ "Plugin Browser" };
        FavoriteDocument favorites;
        friend bool operator==(const Settings&, const Settings&) = default;
    };

}
