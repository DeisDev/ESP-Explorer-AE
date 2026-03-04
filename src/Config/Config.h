#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    struct Settings
    {
        std::string language{ "en" };
        std::uint32_t toggleKey{ 0x2D };
        bool showOnStartup{ false };
        bool noPauseOnFocusLoss{ false };
        bool pauseGameWhenMenuOpen{ false };
        bool verboseLogging{ true };
        bool hideNonPlayable{ true };
        bool hideDeleted{ true };
        bool hideNoName{ true };
        bool listShowPlayable{ true };
        bool listShowNonPlayable{ true };
        bool listShowNamed{ true };
        bool listShowUnnamed{ true };
        bool listShowDeleted{ true };
        bool pluginGlobalSearchMode{ false };
        bool pluginShowUnknownCategories{ false };
        bool autoFocusSearchBars{ true };
        bool showPlayerStatsInStatus{ true };
        bool showMenuResolutionInStatus{ false };
        bool enableGamepadNav{ true };
        bool showFPSInStatus{ true };
        bool rememberWindowPos{ true };
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
        std::string lastActiveTab{ "Plugin Browser" };
        std::vector<std::uint32_t> favorites{};
    };

    class Config
    {
    public:
        static bool Load();
        static bool Save();
        static const Settings& Get();
        static Settings& GetMutable();

    private:
        static inline Settings settings{};
        static inline std::filesystem::path configPath{};
    };
}
