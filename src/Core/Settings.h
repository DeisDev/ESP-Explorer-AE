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
        bool doubleClickGameplayAction{};
        bool compactTableDensity{};
        bool showPlayerStatsInStatus{ false };
        bool showMenuResolutionInStatus{ false };
        MultiCopyFormat multiCopyFormat{ MultiCopyFormat::Lines };
        bool allowGameplayActionsInMainMenu{ false };
        bool componentSubstitution{ true };
        bool includeAmmoWithWeapons{ true };
        int defaultAmmoQuantity{ 100 };
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
        float themeAccentR{ 0.27f };
        float themeAccentG{ 0.94f };
        float themeAccentB{ 0.38f };
        float themeAccentA{ 1.0f };
        float themeWindowR{ 0.03f };
        float themeWindowG{ 0.08f };
        float themeWindowB{ 0.05f };
        float themeWindowA{ 0.96f };
        float themePanelR{ 0.06f };
        float themePanelG{ 0.14f };
        float themePanelB{ 0.09f };
        float themePanelA{ 0.94f };
        bool syncPipboyColor{ false };
        std::string themePresetId{ "default-green" };
        std::string lastActiveTab{ "Plugin Browser" };
        FavoriteDocument favorites;
        friend bool operator==(const Settings&, const Settings&) = default;
    };

}
