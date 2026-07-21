#include "Config/SettingsIni.h"
#include "Config/FavoriteIni.h"
#include "Core/SettingsValidation.h"

#include <SimpleIni.h>

namespace ESPExplorerAE
{
    SettingsReadResult ReadSettingsIni(std::string_view bytes)
    {
        SettingsReadResult result;
        if (bytes.find('\0') != std::string_view::npos) { result.error = "Configuration contains an embedded NUL"; return result; }
        CSimpleIniA ini;
        ini.SetUnicode();
        if (ini.LoadData(bytes.data(), bytes.size()) < 0) { result.error = "INI parse failed"; return result; }
        auto& settings = result.settings;
        if (!ReadFavoriteIni(ini, settings.favorites, result.legacyFavorites)) { result.error = "Unsupported favorite schema"; return result; }

        settings.language = ini.GetValue("General", "sLanguage", "en");
        settings.toggleKey = static_cast<std::uint32_t>(ini.GetLongValue("General", "iToggleKey", Settings{}.toggleKey));
        settings.showOnStartup = ini.GetBoolValue("General", "bShowOnStartup", false);
        settings.pauseGameWhenMenuOpen = ini.GetBoolValue("General", "bPauseGameWhenMenuOpen", false);
        settings.hidePlayerHUDWhenMenuOpen = ini.GetBoolValue("General", "bHidePlayerHUDWhenMenuOpen", false);
        settings.godModeWhenMenuOpen = ini.GetBoolValue("General", "bGodModeWhenMenuOpen", false);
        settings.debugLogging = ini.GetBoolValue("Logging", "bDebugLogging",
                          ini.GetBoolValue("Logging", "bVerboseLogging",
                              ini.GetBoolValue("General", "bVerboseLogging", true)));

        settings.profilePerformance = ini.GetBoolValue("Debug", "bProfilePerformance", false);

        settings.fontSize = static_cast<float>(ini.GetDoubleValue("UI", "fFontSize", 20.0));
        settings.windowAlpha = static_cast<float>(ini.GetDoubleValue("UI", "fWindowAlpha", 0.95));
        settings.rememberWindowPos = ini.GetBoolValue("UI", "bRememberWindowPos", true);
        settings.startupTab = ini.GetValue("UI", "sStartupTab", "__last__");
        settings.firstRunHelpDismissed = ini.GetBoolValue("UI", "bFirstRunHelpDismissed", true);
        settings.actionHistoryWindowX = static_cast<float>(ini.GetDoubleValue("UI", "fActionHistoryWindowX", 200.0));
        settings.actionHistoryWindowY = static_cast<float>(ini.GetDoubleValue("UI", "fActionHistoryWindowY", 160.0));
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
        settings.themePresetId = ini.GetValue("Theme", "sPresetId", "");

        settings.hideNonPlayable = ini.GetBoolValue("Filters", "bHideNonPlayable", true);
        settings.hideDeleted = ini.GetBoolValue("Filters", "bHideDeleted", true);
        settings.hideNoName = ini.GetBoolValue("Filters", "bHideNoName", true);
        settings.listShowPlayable = ini.GetBoolValue("Filters", "bListShowPlayable", true);
        settings.listShowNonPlayable = ini.GetBoolValue("Filters", "bListShowNonPlayable", false);
        settings.listShowNamed = ini.GetBoolValue("Filters", "bListShowNamed", true);
        settings.listShowUnnamed = ini.GetBoolValue("Filters", "bListShowUnnamed", false);
        settings.listShowDeleted = ini.GetBoolValue("Filters", "bListShowDeleted", true);
        settings.advancedRecordFilters = ini.GetValue("Filters", "sAdvancedRecordFilters", "");
        settings.hiddenPlugins = ini.GetValue("Filters", "sHiddenPlugins", "");
        settings.pluginGlobalSearchMode = ini.GetBoolValue("Filters", "bPluginGlobalSearchMode", false);
        settings.pluginShowUnknownCategories = ini.GetBoolValue("Filters", "bPluginShowUnknownCategories", false);
        settings.recentRecordsLimit = (std::clamp)(static_cast<int>(ini.GetLongValue("UI", "iRecentRecordsLimit", 25)), 5, 100);
        settings.autoFocusSearchBars = ini.GetBoolValue("UI", "bAutoFocusSearchBars", true);
        settings.showPlayerStatsInStatus = ini.GetBoolValue("UI", "bShowPlayerStatsInStatus", false);
        settings.showMenuResolutionInStatus = ini.GetBoolValue("UI", "bShowMenuResolutionInStatus", false);
        settings.pluginAdvancedDetailsView = ini.GetBoolValue("UI", "bPluginAdvancedDetailsView", false);
        settings.multiCopyFormat = ValidatedCopyFormat(ini.GetLongValue("UI", "iMultiCopyFormat", static_cast<long>(MultiCopyFormat::Lines)));

        settings.allowGameplayActionsInMainMenu = ini.GetBoolValue("Debug", "bAllowGameplayActionsInMainMenu", false);
        settings.componentSubstitution = ini.GetBoolValue("General", "bComponentSubstitution", true);

        settings.enableGamepadNav = ini.GetBoolValue("Controller", "bEnableGamepadNav", true);
        settings.showLogsTab = ini.GetBoolValue("Logging", "bShowLogsTab", true);


        result.adjusted = ValidateSettings(settings);
        return result;
    }

    bool WriteSettingsIni(const Settings& settings, std::string& bytes)
    {
        auto settingsSnapshot = settings;
        ValidateSettings(settingsSnapshot);
        CSimpleIniA ini;
        ini.SetUnicode();

        ini.SetValue("General", "sLanguage", settingsSnapshot.language.c_str());
        ini.SetLongValue("General", "iToggleKey", static_cast<long>(settingsSnapshot.toggleKey));
        ini.SetBoolValue("General", "bShowOnStartup", settingsSnapshot.showOnStartup);
        ini.SetBoolValue("General", "bPauseGameWhenMenuOpen", settingsSnapshot.pauseGameWhenMenuOpen);
        ini.SetBoolValue("General", "bHidePlayerHUDWhenMenuOpen", settingsSnapshot.hidePlayerHUDWhenMenuOpen);
        ini.SetBoolValue("General", "bGodModeWhenMenuOpen", settingsSnapshot.godModeWhenMenuOpen);

        ini.SetDoubleValue("UI", "fFontSize", settingsSnapshot.fontSize);
        ini.SetDoubleValue("UI", "fWindowAlpha", settingsSnapshot.windowAlpha);
        ini.SetBoolValue("UI", "bRememberWindowPos", settingsSnapshot.rememberWindowPos);
        ini.SetValue("UI", "sStartupTab", settingsSnapshot.startupTab.c_str());
        ini.SetBoolValue("UI", "bFirstRunHelpDismissed", settingsSnapshot.firstRunHelpDismissed);
        ini.SetDoubleValue("UI", "fActionHistoryWindowX", settingsSnapshot.actionHistoryWindowX);
        ini.SetDoubleValue("UI", "fActionHistoryWindowY", settingsSnapshot.actionHistoryWindowY);
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
        ini.SetValue("Theme", "sPresetId", settingsSnapshot.themePresetId.c_str());

        ini.SetBoolValue("Filters", "bHideNonPlayable", settingsSnapshot.hideNonPlayable);
        ini.SetBoolValue("Filters", "bHideDeleted", settingsSnapshot.hideDeleted);
        ini.SetBoolValue("Filters", "bHideNoName", settingsSnapshot.hideNoName);
        ini.SetBoolValue("Filters", "bListShowPlayable", settingsSnapshot.listShowPlayable);
        ini.SetBoolValue("Filters", "bListShowNonPlayable", settingsSnapshot.listShowNonPlayable);
        ini.SetBoolValue("Filters", "bListShowNamed", settingsSnapshot.listShowNamed);
        ini.SetBoolValue("Filters", "bListShowUnnamed", settingsSnapshot.listShowUnnamed);
        ini.SetBoolValue("Filters", "bListShowDeleted", settingsSnapshot.listShowDeleted);
        ini.SetValue("Filters", "sAdvancedRecordFilters", settingsSnapshot.advancedRecordFilters.c_str());
        ini.SetValue("Filters", "sHiddenPlugins", settingsSnapshot.hiddenPlugins.c_str());
        ini.SetBoolValue("Filters", "bPluginGlobalSearchMode", settingsSnapshot.pluginGlobalSearchMode);
        ini.SetBoolValue("Filters", "bPluginShowUnknownCategories", settingsSnapshot.pluginShowUnknownCategories);
        ini.SetLongValue("UI", "iRecentRecordsLimit", (std::clamp)(settingsSnapshot.recentRecordsLimit, 5, 100));
        ini.SetBoolValue("UI", "bAutoFocusSearchBars", settingsSnapshot.autoFocusSearchBars);
        ini.SetBoolValue("UI", "bShowPlayerStatsInStatus", settingsSnapshot.showPlayerStatsInStatus);
        ini.SetBoolValue("UI", "bShowMenuResolutionInStatus", settingsSnapshot.showMenuResolutionInStatus);
        ini.SetBoolValue("UI", "bPluginAdvancedDetailsView", settingsSnapshot.pluginAdvancedDetailsView);
        ini.SetLongValue("UI", "iMultiCopyFormat", static_cast<long>(settingsSnapshot.multiCopyFormat));

        ini.SetBoolValue("Debug", "bAllowGameplayActionsInMainMenu", settingsSnapshot.allowGameplayActionsInMainMenu);
        ini.SetBoolValue("Debug", "bProfilePerformance", settingsSnapshot.profilePerformance);
        ini.SetBoolValue("General", "bComponentSubstitution", settingsSnapshot.componentSubstitution);

        ini.SetBoolValue("Controller", "bEnableGamepadNav", settingsSnapshot.enableGamepadNav);

        ini.SetBoolValue("Logging", "bDebugLogging", settingsSnapshot.debugLogging);
        ini.SetBoolValue("Logging", "bShowLogsTab", settingsSnapshot.showLogsTab);

        WriteFavoriteIni(ini, settingsSnapshot.favorites);
        std::string serialized;
        if (ini.Save(serialized) < 0) return false;
        bytes = std::move(serialized);
        return true;
    }
}
