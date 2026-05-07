#include "GUI/ThemeManager.h"

#include "Config/Config.h"
#include "Logging/Logger.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ESPExplorerAE
{
    namespace
    {
        constexpr std::string_view kThemeSection = "Theme";

        const ThemePreset kDefaultTheme{
            .id = "default-green",
            .name = "Default Green",
            .nameKey = "sPresetDefaultGreen",
            .accentR = 0.27f,
            .accentG = 0.94f,
            .accentB = 0.38f,
            .accentA = 1.0f,
            .windowR = 0.03f,
            .windowG = 0.08f,
            .windowB = 0.05f,
            .windowA = 0.96f,
            .panelR = 0.06f,
            .panelG = 0.14f,
            .panelB = 0.09f,
            .panelA = 0.94f,
            .builtIn = true
        };

        std::vector<ThemePreset> availableThemes{};
        bool themesLoaded{ false };

        std::string ToLower(std::string_view value)
        {
            std::string result(value);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return result;
        }

        bool NearlyEqual(float left, float right)
        {
            return std::fabs(left - right) <= 0.01f;
        }

        bool SameColors(const Settings& settings, const ThemePreset& theme)
        {
            return NearlyEqual(settings.themeAccentR, theme.accentR) &&
                NearlyEqual(settings.themeAccentG, theme.accentG) &&
                NearlyEqual(settings.themeAccentB, theme.accentB) &&
                NearlyEqual(settings.themeAccentA, theme.accentA) &&
                NearlyEqual(settings.themeWindowR, theme.windowR) &&
                NearlyEqual(settings.themeWindowG, theme.windowG) &&
                NearlyEqual(settings.themeWindowB, theme.windowB) &&
                NearlyEqual(settings.themeWindowA, theme.windowA) &&
                NearlyEqual(settings.themePanelR, theme.panelR) &&
                NearlyEqual(settings.themePanelG, theme.panelG) &&
                NearlyEqual(settings.themePanelB, theme.panelB) &&
                NearlyEqual(settings.themePanelA, theme.panelA);
        }

        bool ParseFloat(const char* value, float& out)
        {
            if (!value || value[0] == '\0') {
                return false;
            }

            char* end = nullptr;
            const float parsed = std::strtof(value, &end);
            if (end == value || (end && end[0] != '\0')) {
                return false;
            }

            out = std::clamp(parsed, 0.0f, 1.0f);
            return true;
        }

        bool ReadColor(CSimpleIniA& ini, const char* key, float& out)
        {
            return ParseFloat(ini.GetValue(kThemeSection.data(), key, nullptr), out);
        }

        bool LoadThemeFile(const std::filesystem::path& path, ThemePreset& out)
        {
            CSimpleIniA ini;
            ini.SetUnicode();

            if (ini.LoadFile(path.string().c_str()) < 0) {
                return false;
            }

            const char* id = ini.GetValue(kThemeSection.data(), "sId", "");
            if (!id || id[0] == '\0') {
                return false;
            }

            ThemePreset theme{};
            theme.id = id;
            theme.name = ini.GetValue(kThemeSection.data(), "sName", id);
            theme.nameKey = ini.GetValue(kThemeSection.data(), "sNameKey", "");

            if (!ReadColor(ini, "fAccentR", theme.accentR) ||
                !ReadColor(ini, "fAccentG", theme.accentG) ||
                !ReadColor(ini, "fAccentB", theme.accentB) ||
                !ReadColor(ini, "fAccentA", theme.accentA) ||
                !ReadColor(ini, "fWindowR", theme.windowR) ||
                !ReadColor(ini, "fWindowG", theme.windowG) ||
                !ReadColor(ini, "fWindowB", theme.windowB) ||
                !ReadColor(ini, "fWindowA", theme.windowA) ||
                !ReadColor(ini, "fPanelR", theme.panelR) ||
                !ReadColor(ini, "fPanelG", theme.panelG) ||
                !ReadColor(ini, "fPanelB", theme.panelB) ||
                !ReadColor(ini, "fPanelA", theme.panelA)) {
                return false;
            }

            out = std::move(theme);
            return true;
        }
    }

    std::filesystem::path ThemeManager::ResolveThemesDirectory()
    {
        const auto runtimePath = std::filesystem::path("Data/Interface/ESPExplorerAE/themes");
        if (std::filesystem::exists(runtimePath)) {
            return runtimePath;
        }

        return std::filesystem::path("dist/themes");
    }

    void ThemeManager::ReloadAvailableThemes()
    {
        std::vector<ThemePreset> loadedThemes;
        loadedThemes.push_back(kDefaultTheme);

        const auto directory = ResolveThemesDirectory();
        if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory)) {
            std::vector<ThemePreset> fileThemes;

            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto extension = entry.path().extension().string();
                if (_stricmp(extension.c_str(), ".ini") != 0) {
                    continue;
                }

                ThemePreset theme{};
                if (!LoadThemeFile(entry.path(), theme)) {
                    Logger::Warn("Skipped invalid theme file: " + entry.path().string());
                    continue;
                }

                if (ToLower(theme.id) == ToLower(kDefaultTheme.id)) {
                    continue;
                }

                const auto duplicate = std::ranges::find_if(fileThemes, [&](const ThemePreset& existing) {
                    return ToLower(existing.id) == ToLower(theme.id);
                });
                if (duplicate != fileThemes.end()) {
                    Logger::Warn("Skipped duplicate theme id: " + theme.id);
                    continue;
                }

                fileThemes.push_back(std::move(theme));
            }

            std::ranges::sort(fileThemes, [](const ThemePreset& left, const ThemePreset& right) {
                const auto leftLabel = left.name.empty() ? left.id : left.name;
                const auto rightLabel = right.name.empty() ? right.id : right.name;
                return _stricmp(leftLabel.c_str(), rightLabel.c_str()) < 0;
            });

            loadedThemes.insert(loadedThemes.end(), fileThemes.begin(), fileThemes.end());
        }

        availableThemes = std::move(loadedThemes);
        themesLoaded = true;
    }

    const std::vector<ThemePreset>& ThemeManager::GetAvailableThemes()
    {
        if (!themesLoaded) {
            ReloadAvailableThemes();
        }

        return availableThemes;
    }

    const ThemePreset& ThemeManager::GetDefaultTheme()
    {
        return kDefaultTheme;
    }

    const ThemePreset* ThemeManager::FindThemeById(std::string_view id)
    {
        if (id.empty()) {
            return nullptr;
        }

        const auto normalized = ToLower(id);
        const auto& themes = GetAvailableThemes();
        const auto match = std::ranges::find_if(themes, [&](const ThemePreset& theme) {
            return ToLower(theme.id) == normalized;
        });
        return match == themes.end() ? nullptr : &*match;
    }

    const ThemePreset* ThemeManager::FindMatchingTheme(const Settings& settings)
    {
        const auto& themes = GetAvailableThemes();
        const auto match = std::ranges::find_if(themes, [&](const ThemePreset& theme) {
            return SameColors(settings, theme);
        });
        return match == themes.end() ? nullptr : &*match;
    }

    void ThemeManager::ApplyTheme(Settings& settings, const ThemePreset& theme)
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
