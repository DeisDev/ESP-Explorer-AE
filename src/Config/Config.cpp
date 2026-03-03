#include "Config/Config.h"

#include <charconv>
#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
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
        settings.verboseLogging = ini.GetBoolValue("General", "bVerboseLogging", true);

        settings.fontSize = static_cast<float>(ini.GetDoubleValue("UI", "fFontSize", 16.0));
        settings.windowAlpha = static_cast<float>(ini.GetDoubleValue("UI", "fWindowAlpha", 0.95));
        settings.rememberWindowPos = ini.GetBoolValue("UI", "bRememberWindowPos", true);
        settings.windowX = static_cast<float>(ini.GetDoubleValue("UI", "fWindowX", 100.0));
        settings.windowY = static_cast<float>(ini.GetDoubleValue("UI", "fWindowY", 100.0));
        settings.windowW = static_cast<float>(ini.GetDoubleValue("UI", "fWindowW", 1200.0));
        settings.windowH = static_cast<float>(ini.GetDoubleValue("UI", "fWindowH", 760.0));
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

        settings.hideNonPlayable = ini.GetBoolValue("Filters", "bHideNonPlayable", true);
        settings.hideDeleted = ini.GetBoolValue("Filters", "bHideDeleted", true);
        settings.hideNoName = ini.GetBoolValue("Filters", "bHideNoName", true);
        settings.listShowPlayable = ini.GetBoolValue("Filters", "bListShowPlayable", true);
        settings.listShowNonPlayable = ini.GetBoolValue("Filters", "bListShowNonPlayable", true);
        settings.listShowNamed = ini.GetBoolValue("Filters", "bListShowNamed", true);
        settings.listShowUnnamed = ini.GetBoolValue("Filters", "bListShowUnnamed", true);
        settings.listShowDeleted = ini.GetBoolValue("Filters", "bListShowDeleted", true);
        settings.pluginGlobalSearchMode = ini.GetBoolValue("Filters", "bPluginGlobalSearchMode", false);
        settings.pluginShowUnknownCategories = ini.GetBoolValue("Filters", "bPluginShowUnknownCategories", false);
        settings.pluginSearchCaseSensitive = ini.GetBoolValue("Filters", "bPluginSearchCaseSensitive", false);
        settings.itemSearchCaseSensitive = ini.GetBoolValue("Filters", "bItemSearchCaseSensitive", false);
        settings.npcSearchCaseSensitive = ini.GetBoolValue("Filters", "bNPCSearchCaseSensitive", false);
        settings.objectSearchCaseSensitive = ini.GetBoolValue("Filters", "bObjectSearchCaseSensitive", false);
        settings.spellPerkSearchCaseSensitive = ini.GetBoolValue("Filters", "bSpellPerkSearchCaseSensitive", false);

        settings.enableGamepadNav = ini.GetBoolValue("Controller", "bEnableGamepadNav", true);

        settings.favorites = ParseFavorites(ini.GetValue("Favorites", "sFormIDs", ""));

        return true;
    }

    bool Config::Save()
    {
        if (configPath.empty()) {
            configPath = ResolveConfigPath();
        }

        std::filesystem::create_directories(configPath.parent_path());

        CSimpleIniA ini;
        ini.SetUnicode();

        ini.SetValue("General", "sLanguage", settings.language.c_str());
        ini.SetLongValue("General", "iToggleKey", static_cast<long>(settings.toggleKey));
        ini.SetBoolValue("General", "bShowOnStartup", settings.showOnStartup);
        ini.SetBoolValue("General", "bNoPauseOnFocusLoss", settings.noPauseOnFocusLoss);
        ini.SetBoolValue("General", "bVerboseLogging", settings.verboseLogging);

        ini.SetDoubleValue("UI", "fFontSize", settings.fontSize);
        ini.SetDoubleValue("UI", "fWindowAlpha", settings.windowAlpha);
        ini.SetBoolValue("UI", "bRememberWindowPos", settings.rememberWindowPos);
        ini.SetDoubleValue("UI", "fWindowX", settings.windowX);
        ini.SetDoubleValue("UI", "fWindowY", settings.windowY);
        ini.SetDoubleValue("UI", "fWindowW", settings.windowW);
        ini.SetDoubleValue("UI", "fWindowH", settings.windowH);
        ini.SetBoolValue("UI", "bShowFPSInStatus", settings.showFPSInStatus);
        ini.SetValue("UI", "sLastActiveTab", settings.lastActiveTab.c_str());

        ini.SetDoubleValue("Theme", "fAccentR", settings.themeAccentR);
        ini.SetDoubleValue("Theme", "fAccentG", settings.themeAccentG);
        ini.SetDoubleValue("Theme", "fAccentB", settings.themeAccentB);
        ini.SetDoubleValue("Theme", "fAccentA", settings.themeAccentA);
        ini.SetDoubleValue("Theme", "fWindowR", settings.themeWindowR);
        ini.SetDoubleValue("Theme", "fWindowG", settings.themeWindowG);
        ini.SetDoubleValue("Theme", "fWindowB", settings.themeWindowB);
        ini.SetDoubleValue("Theme", "fWindowA", settings.themeWindowA);
        ini.SetDoubleValue("Theme", "fPanelR", settings.themePanelR);
        ini.SetDoubleValue("Theme", "fPanelG", settings.themePanelG);
        ini.SetDoubleValue("Theme", "fPanelB", settings.themePanelB);
        ini.SetDoubleValue("Theme", "fPanelA", settings.themePanelA);

        ini.SetBoolValue("Filters", "bHideNonPlayable", settings.hideNonPlayable);
        ini.SetBoolValue("Filters", "bHideDeleted", settings.hideDeleted);
        ini.SetBoolValue("Filters", "bHideNoName", settings.hideNoName);
        ini.SetBoolValue("Filters", "bListShowPlayable", settings.listShowPlayable);
        ini.SetBoolValue("Filters", "bListShowNonPlayable", settings.listShowNonPlayable);
        ini.SetBoolValue("Filters", "bListShowNamed", settings.listShowNamed);
        ini.SetBoolValue("Filters", "bListShowUnnamed", settings.listShowUnnamed);
        ini.SetBoolValue("Filters", "bListShowDeleted", settings.listShowDeleted);
        ini.SetBoolValue("Filters", "bPluginGlobalSearchMode", settings.pluginGlobalSearchMode);
        ini.SetBoolValue("Filters", "bPluginShowUnknownCategories", settings.pluginShowUnknownCategories);
        ini.SetBoolValue("Filters", "bPluginSearchCaseSensitive", settings.pluginSearchCaseSensitive);
        ini.SetBoolValue("Filters", "bItemSearchCaseSensitive", settings.itemSearchCaseSensitive);
        ini.SetBoolValue("Filters", "bNPCSearchCaseSensitive", settings.npcSearchCaseSensitive);
        ini.SetBoolValue("Filters", "bObjectSearchCaseSensitive", settings.objectSearchCaseSensitive);
        ini.SetBoolValue("Filters", "bSpellPerkSearchCaseSensitive", settings.spellPerkSearchCaseSensitive);

        ini.SetBoolValue("Controller", "bEnableGamepadNav", settings.enableGamepadNav);

        const auto favorites = SerializeFavorites(settings.favorites);
        ini.SetValue("Favorites", "sFormIDs", favorites.c_str());

        return ini.SaveFile(configPath.string().c_str()) >= 0;
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
