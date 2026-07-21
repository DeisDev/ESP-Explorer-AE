#include "Config/ThemeStore.h"

#include <SimpleIni.h>
#include <fstream>

#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ESPExplorerAE
{
    namespace
    {
        constexpr std::string_view kThemeSection = "Theme";

        std::shared_ptr<const std::vector<ThemePreset>> availableThemes;

        std::string ToLower(std::string_view value)
        {
            std::string result(value);
            for (auto& ch : result) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            return result;
        }

        bool ParseFloat(const char* value, float& out)
        {
            if (!value || value[0] == '\0') {
                return false;
            }

            char* end = nullptr;
            const float parsed = std::strtof(value, &end);
            if (end == value || (end && end[0] != '\0') || !std::isfinite(parsed)) {
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

            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (file.bad() || bytes.find('\0') != std::string::npos || ini.LoadData(bytes) < 0) return false;

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

    std::filesystem::path ThemeStore::ResolveThemesDirectory()
    {
        const auto runtimePath = std::filesystem::path("Data/Interface/ESPExplorerAE/themes");
        std::error_code error;
        if (std::filesystem::exists(runtimePath, error) || error) {
            return runtimePath;
        }

        return std::filesystem::path("dist/themes");
    }

    std::vector<ThemePreset> ThemeStore::ReadDirectory(const std::filesystem::path& directory, const Diagnostic& diagnostic)
    {
        std::vector<ThemePreset> loadedThemes;
        loadedThemes.push_back(DefaultTheme);

        try {
        if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory)) {
            std::vector<ThemePreset> fileThemes;

            std::vector<std::filesystem::path> paths;
            for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                if (entry.is_regular_file() && ToLower(entry.path().extension().string()) == ".ini") paths.push_back(entry.path());
            }
            std::ranges::sort(paths);
            for (const auto& path : paths) {

                ThemePreset theme{};
                if (!LoadThemeFile(path, theme)) {
                    if (diagnostic) diagnostic("Skipped invalid theme file: " + path.string());
                    continue;
                }

                if (ToLower(theme.id) == ToLower(DefaultTheme.id)) {
                    continue;
                }

                const auto duplicate = std::ranges::find_if(fileThemes, [&](const ThemePreset& existing) {
                    return ToLower(existing.id) == ToLower(theme.id);
                });
                if (duplicate != fileThemes.end()) {
                    if (diagnostic) diagnostic("Skipped duplicate theme id: " + theme.id);
                    continue;
                }

                fileThemes.push_back(std::move(theme));
            }

            std::ranges::sort(fileThemes, [](const ThemePreset& left, const ThemePreset& right) {
                const auto leftLabel = left.name.empty() ? left.id : left.name;
                const auto rightLabel = right.name.empty() ? right.id : right.name;
                const auto lhs = ToLower(leftLabel), rhs = ToLower(rightLabel);
                return lhs == rhs ? left.id < right.id : lhs < rhs;
            });

            loadedThemes.insert(loadedThemes.end(), fileThemes.begin(), fileThemes.end());
        }

        } catch (const std::filesystem::filesystem_error& error) {
            if (diagnostic) diagnostic("Cannot read themes: " + std::string(error.what()));
        }
        return loadedThemes;
    }

    void ThemeStore::ReloadAvailableThemes(const Diagnostic& diagnostic)
    {
        availableThemes = std::make_shared<const std::vector<ThemePreset>>(ReadDirectory(ResolveThemesDirectory(), diagnostic));
    }

    std::shared_ptr<const std::vector<ThemePreset>> ThemeStore::Snapshot(const Diagnostic& diagnostic)
    {
        if (!availableThemes) ReloadAvailableThemes(diagnostic);
        return availableThemes;
    }
}
