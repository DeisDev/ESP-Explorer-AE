#include "Config/Config.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        constexpr auto kSaveDebounce = std::chrono::milliseconds(300);

        Settings pendingSettings{};
        std::filesystem::path pendingConfigPath{};
        std::chrono::steady_clock::time_point pendingSaveDeadline{};
        bool pendingSaveRequested{ false };

        std::vector<std::uint32_t> ParseFavorites(const std::string_view csv)
        {
            std::vector<std::uint32_t> result;
            std::size_t start = 0;

            while (start < csv.size()) {
                const auto end = csv.find(',', start);
                const auto token = csv.substr(start, end == std::string_view::npos ? std::string_view::npos : (end - start));

                if (!token.empty()) {
                    std::uint32_t value = 0;
                    const auto beginPtr = token.data();
                    const auto endPtr = token.data() + token.size();
                    const auto [ptr, ec] = std::from_chars(beginPtr, endPtr, value, 16);
                    if (ec == std::errc{} && ptr == endPtr) {
                        result.push_back(value);
                    }
                }

                if (end == std::string_view::npos) {
                    break;
                }
                start = end + 1;
            }

            return result;
        }

        std::string SerializeFavorites(const std::vector<std::uint32_t>& favorites)
        {
            std::string result;
            result.reserve(favorites.size() * 9);

            char buffer[16]{};

            for (std::size_t index = 0; index < favorites.size(); ++index) {
                if (index != 0) {
                    result.push_back(',');
                }

                std::snprintf(buffer, sizeof(buffer), "%08X", favorites[index]);
                result.append(buffer);
            }

            return result;
        }
    }

    static std::filesystem::path ResolveConfigPath()
    {
        return std::filesystem::path("Data/F4SE/Plugins/ESPExplorerAE.ini");
    }

    static bool SaveSnapshot(const Settings& settingsSnapshot, const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());

        CSimpleIniA ini;
        ini.SetUnicode();

        ini.SetValue("General", "sLanguage", settingsSnapshot.language.c_str());
        ini.SetLongValue("General", "iToggleKey", static_cast<long>(settingsSnapshot.toggleKey));
        ini.SetBoolValue("General", "bShowOnStartup", settingsSnapshot.showOnStartup);
        ini.SetBoolValue("General", "bNoPauseOnFocusLoss", settingsSnapshot.noPauseOnFocusLoss);
        ini.SetBoolValue("General", "bPauseGameWhenMenuOpen", settingsSnapshot.pauseGameWhenMenuOpen);
        ini.SetBoolValue("General", "bHidePlayerHUDWhenMenuOpen", settingsSnapshot.hidePlayerHUDWhenMenuOpen);
        ini.SetBoolValue("General", "bVerboseLogging", settingsSnapshot.verboseLogging);

        ini.SetDoubleValue("UI", "fFontSize", settingsSnapshot.fontSize);
        ini.SetDoubleValue("UI", "fWindowAlpha", settingsSnapshot.windowAlpha);
        ini.SetBoolValue("UI", "bRememberWindowPos", settingsSnapshot.rememberWindowPos);
        ini.SetValue("UI", "sStartupTab", settingsSnapshot.startupTab.c_str());
        ini.SetDoubleValue("UI", "fWindowX", settingsSnapshot.windowX);
        ini.SetDoubleValue("UI", "fWindowY", settingsSnapshot.windowY);
        ini.SetDoubleValue("UI", "fWindowW", settingsSnapshot.windowW);
        ini.SetDoubleValue("UI", "fWindowH", settingsSnapshot.windowH);
        ini.SetBoolValue("UI", "bShowFPSInStatus", settingsSnapshot.showFPSInStatus);
        ini.SetValue("UI", "sLastActiveTab", settingsSnapshot.lastActiveTab.c_str());

        ini.SetDoubleValue("Theme", "fAccentR", settingsSnapshot.themeAccentR);
        ini.SetDoubleValue("Theme", "fAccentG", settingsSnapshot.themeAccentG);
        ini.SetDoubleValue("Theme", "fAccentB", settingsSnapshot.themeAccentB);
        ini.SetDoubleValue("Theme", "fAccentA", settingsSnapshot.themeAccentA);
        ini.SetDoubleValue("Theme", "fWindowR", settingsSnapshot.themeWindowR);
        ini.SetDoubleValue("Theme", "fWindowG", settingsSnapshot.themeWindowG);
        ini.SetDoubleValue("Theme", "fWindowB", settingsSnapshot.themeWindowB);
        ini.SetDoubleValue("Theme", "fWindowA", settingsSnapshot.themeWindowA);
        ini.SetDoubleValue("Theme", "fPanelR", settingsSnapshot.themePanelR);
        ini.SetDoubleValue("Theme", "fPanelG", settingsSnapshot.themePanelG);
        ini.SetDoubleValue("Theme", "fPanelB", settingsSnapshot.themePanelB);
        ini.SetDoubleValue("Theme", "fPanelA", settingsSnapshot.themePanelA);
        ini.SetBoolValue("Theme", "bSyncPipboyColor", settingsSnapshot.syncPipboyColor);

        ini.SetBoolValue("Filters", "bHideNonPlayable", settingsSnapshot.hideNonPlayable);
        ini.SetBoolValue("Filters", "bHideDeleted", settingsSnapshot.hideDeleted);
        ini.SetBoolValue("Filters", "bHideNoName", settingsSnapshot.hideNoName);
        ini.SetBoolValue("Filters", "bListShowPlayable", settingsSnapshot.listShowPlayable);
        ini.SetBoolValue("Filters", "bListShowNonPlayable", settingsSnapshot.listShowNonPlayable);
        ini.SetBoolValue("Filters", "bListShowNamed", settingsSnapshot.listShowNamed);
        ini.SetBoolValue("Filters", "bListShowUnnamed", settingsSnapshot.listShowUnnamed);
        ini.SetBoolValue("Filters", "bListShowDeleted", settingsSnapshot.listShowDeleted);
        ini.SetBoolValue("Filters", "bPluginGlobalSearchMode", settingsSnapshot.pluginGlobalSearchMode);
        ini.SetBoolValue("Filters", "bPluginShowUnknownCategories", settingsSnapshot.pluginShowUnknownCategories);
        ini.SetLongValue("UI", "iRecentRecordsLimit", (std::clamp)(settingsSnapshot.recentRecordsLimit, 5, 100));
        ini.SetBoolValue("UI", "bAutoFocusSearchBars", settingsSnapshot.autoFocusSearchBars);
        ini.SetBoolValue("UI", "bShowPlayerStatsInStatus", settingsSnapshot.showPlayerStatsInStatus);
        ini.SetBoolValue("UI", "bShowMenuResolutionInStatus", settingsSnapshot.showMenuResolutionInStatus);
        ini.SetBoolValue("UI", "bPluginAdvancedDetailsView", settingsSnapshot.pluginAdvancedDetailsView);

        ini.SetBoolValue("Controller", "bEnableGamepadNav", settingsSnapshot.enableGamepadNav);

        ini.SetBoolValue("Logging", "bVerboseLogging", settingsSnapshot.verboseLogging);
        ini.SetBoolValue("Logging", "bShowLogsTab", settingsSnapshot.showLogsTab);

        const auto favorites = SerializeFavorites(settingsSnapshot.favorites);
        ini.SetValue("Favorites", "sFormIDs", favorites.c_str());

        return ini.SaveFile(path.string().c_str()) >= 0;
    }

    bool Config::Load()
    {
        configPath = ResolveConfigPath();

        CSimpleIniA ini;
        ini.SetUnicode();

        if (!std::filesystem::exists(configPath)) {
            Save();
            return true;
        }

        if (ini.LoadFile(configPath.string().c_str()) < 0) {
            return false;
        }

        settings.language = ini.GetValue("General", "sLanguage", "en");
        settings.toggleKey = static_cast<std::uint32_t>(ini.GetLongValue("General", "iToggleKey", 0x2D));
        settings.showOnStartup = ini.GetBoolValue("General", "bShowOnStartup", false);
        settings.noPauseOnFocusLoss = ini.GetBoolValue("General", "bNoPauseOnFocusLoss", false);
        settings.pauseGameWhenMenuOpen = ini.GetBoolValue("General", "bPauseGameWhenMenuOpen", false);
        settings.hidePlayerHUDWhenMenuOpen = ini.GetBoolValue("General", "bHidePlayerHUDWhenMenuOpen", true);
        settings.verboseLogging = ini.GetValue("Logging", "bVerboseLogging", nullptr) ?
                          ini.GetBoolValue("Logging", "bVerboseLogging", true) :
                          ini.GetBoolValue("General", "bVerboseLogging", true);

        settings.fontSize = static_cast<float>(ini.GetDoubleValue("UI", "fFontSize", 20.0));
        settings.windowAlpha = static_cast<float>(ini.GetDoubleValue("UI", "fWindowAlpha", 0.95));
        settings.rememberWindowPos = ini.GetBoolValue("UI", "bRememberWindowPos", true);
        settings.startupTab = ini.GetValue("UI", "sStartupTab", "__last__");
        settings.windowX = static_cast<float>(ini.GetDoubleValue("UI", "fWindowX", 100.0));
        settings.windowY = static_cast<float>(ini.GetDoubleValue("UI", "fWindowY", 100.0));
        settings.windowW = static_cast<float>(ini.GetDoubleValue("UI", "fWindowW", 1440.0));
        settings.windowH = static_cast<float>(ini.GetDoubleValue("UI", "fWindowH", 810.0));
        settings.showFPSInStatus = ini.GetBoolValue("UI", "bShowFPSInStatus", true);
        settings.lastActiveTab = ini.GetValue("UI", "sLastActiveTab", "Plugin Browser");

        settings.themeAccentR = static_cast<float>(ini.GetDoubleValue("Theme", "fAccentR", settings.themeAccentR));
        settings.themeAccentG = static_cast<float>(ini.GetDoubleValue("Theme", "fAccentG", settings.themeAccentG));
        settings.themeAccentB = static_cast<float>(ini.GetDoubleValue("Theme", "fAccentB", settings.themeAccentB));
        settings.themeAccentA = static_cast<float>(ini.GetDoubleValue("Theme", "fAccentA", settings.themeAccentA));
        settings.themeWindowR = static_cast<float>(ini.GetDoubleValue("Theme", "fWindowR", settings.themeWindowR));
        settings.themeWindowG = static_cast<float>(ini.GetDoubleValue("Theme", "fWindowG", settings.themeWindowG));
        settings.themeWindowB = static_cast<float>(ini.GetDoubleValue("Theme", "fWindowB", settings.themeWindowB));
        settings.themeWindowA = static_cast<float>(ini.GetDoubleValue("Theme", "fWindowA", settings.themeWindowA));
        settings.themePanelR = static_cast<float>(ini.GetDoubleValue("Theme", "fPanelR", settings.themePanelR));
        settings.themePanelG = static_cast<float>(ini.GetDoubleValue("Theme", "fPanelG", settings.themePanelG));
        settings.themePanelB = static_cast<float>(ini.GetDoubleValue("Theme", "fPanelB", settings.themePanelB));
        settings.themePanelA = static_cast<float>(ini.GetDoubleValue("Theme", "fPanelA", settings.themePanelA));
        settings.syncPipboyColor = ini.GetBoolValue("Theme", "bSyncPipboyColor", false);

        settings.hideNonPlayable = ini.GetBoolValue("Filters", "bHideNonPlayable", true);
        settings.hideDeleted = ini.GetBoolValue("Filters", "bHideDeleted", true);
        settings.hideNoName = ini.GetBoolValue("Filters", "bHideNoName", true);
        settings.listShowPlayable = ini.GetBoolValue("Filters", "bListShowPlayable", true);
        settings.listShowNonPlayable = ini.GetBoolValue("Filters", "bListShowNonPlayable", false);
        settings.listShowNamed = ini.GetBoolValue("Filters", "bListShowNamed", true);
        settings.listShowUnnamed = ini.GetBoolValue("Filters", "bListShowUnnamed", false);
        settings.listShowDeleted = ini.GetBoolValue("Filters", "bListShowDeleted", true);
        settings.pluginGlobalSearchMode = ini.GetBoolValue("Filters", "bPluginGlobalSearchMode", false);
        settings.pluginShowUnknownCategories = ini.GetBoolValue("Filters", "bPluginShowUnknownCategories", false);
        settings.recentRecordsLimit = (std::clamp)(static_cast<int>(ini.GetLongValue("UI", "iRecentRecordsLimit", 25)), 5, 100);
        settings.autoFocusSearchBars = ini.GetBoolValue("UI", "bAutoFocusSearchBars", true);
        settings.showPlayerStatsInStatus = ini.GetBoolValue("UI", "bShowPlayerStatsInStatus", false);
        settings.showMenuResolutionInStatus = ini.GetBoolValue("UI", "bShowMenuResolutionInStatus", false);
        settings.pluginAdvancedDetailsView = ini.GetBoolValue("UI", "bPluginAdvancedDetailsView", false);

        settings.enableGamepadNav = ini.GetBoolValue("Controller", "bEnableGamepadNav", true);
        settings.showLogsTab = ini.GetBoolValue("Logging", "bShowLogsTab", true);

        settings.favorites = ParseFavorites(ini.GetValue("Favorites", "sFormIDs", ""));

        return true;
    }

    bool Config::Save()
    {
        if (configPath.empty()) {
            configPath = ResolveConfigPath();
        }

        pendingSettings = settings;
        pendingConfigPath = configPath;
        pendingSaveRequested = false;

        return SaveSnapshot(settings, configPath);
    }

    void Config::RequestSave()
    {
        if (configPath.empty()) {
            configPath = ResolveConfigPath();
        }

        pendingSettings = settings;
        pendingConfigPath = configPath;
        pendingSaveDeadline = std::chrono::steady_clock::now() + kSaveDebounce;
        pendingSaveRequested = true;
    }

    bool Config::FlushPendingSaveIfDue()
    {
        if (!pendingSaveRequested || std::chrono::steady_clock::now() < pendingSaveDeadline) {
            return false;
        }

        return FlushPendingSave();
    }

    bool Config::FlushPendingSave()
    {
        if (!pendingSaveRequested) {
            return false;
        }

        pendingSaveRequested = false;
        return SaveSnapshot(pendingSettings, pendingConfigPath);
    }

    void Config::ResetToDefaults()
    {
        settings = Settings{};
    }

    const Settings& Config::Get()
    {
        return settings;
    }

    Settings& Config::GetMutable()
    {
        return settings;
    }
}
