#include "Localization/Language.h"


#include <algorithm>
#include <cctype>

namespace ESPExplorerAE
{
    namespace
    {
        constexpr std::string_view kLanguageMetaSection = "Language";
        constexpr std::string_view kLanguageNameKey = "sName";
        constexpr std::string_view kLanguageFontsKey = "sFontFiles";
        constexpr std::string_view kLanguageRangesKey = "sGlyphRanges";

        std::filesystem::path ResolveLanguageDirectory()
        {
            const auto runtimePath = std::filesystem::path("Data/Interface/ESPExplorerAE/lang");
            if (std::filesystem::exists(runtimePath)) {
                return runtimePath;
            }

            return std::filesystem::path("dist/lang");
        }

        std::filesystem::path ResolveLanguagePath(std::string_view code)
        {
            const auto basePath = ResolveLanguageDirectory();
            return basePath / (std::string(code) + ".ini");
        }

        std::string Trim(std::string_view value)
        {
            std::size_t start = 0;
            while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
                ++start;
            }

            std::size_t end = value.size();
            while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
                --end;
            }

            return std::string(value.substr(start, end - start));
        }

        std::vector<std::string> ParseCsv(std::string_view value)
        {
            std::vector<std::string> result;
            std::size_t start = 0;
            while (start <= value.size()) {
                const auto end = value.find(',', start);
                const auto token = Trim(value.substr(start, end == std::string_view::npos ? value.size() - start : end - start));
                if (!token.empty()) {
                    result.push_back(token);
                }

                if (end == std::string_view::npos) {
                    break;
                }

                start = end + 1;
            }

            return result;
        }

        bool LoadLanguageFile(const std::filesystem::path& path, std::unordered_map<std::string, std::string>& out, Language::Definition* definition)
        {
            CSimpleIniA ini;
            ini.SetUnicode();

            if (definition) {
                definition->code = path.stem().string();
                definition->displayName = definition->code;
                definition->fontFiles.clear();
                definition->glyphRanges.clear();
            }

            if (ini.LoadFile(path.string().c_str()) < 0) {
                return false;
            }

            if (definition) {
                const char* displayName = ini.GetValue(kLanguageMetaSection.data(), kLanguageNameKey.data(), "");
                const char* fontFiles = ini.GetValue(kLanguageMetaSection.data(), kLanguageFontsKey.data(), "");
                const char* glyphRanges = ini.GetValue(kLanguageMetaSection.data(), kLanguageRangesKey.data(), "");

                if (displayName && displayName[0] != '\0') {
                    definition->displayName = displayName;
                }

                definition->fontFiles = ParseCsv(fontFiles);
                definition->glyphRanges = ParseCsv(glyphRanges);
            }

            CSimpleIniA::TNamesDepend sections;
            ini.GetAllSections(sections);

            for (const auto& section : sections) {
                if (_stricmp(section.pItem, kLanguageMetaSection.data()) == 0) {
                    continue;
                }

                CSimpleIniA::TNamesDepend keys;
                ini.GetAllKeys(section.pItem, keys);

                for (const auto& key : keys) {
                    const auto value = ini.GetValue(section.pItem, key.pItem, "");
                    out[std::string(section.pItem) + "." + std::string(key.pItem)] = value;
                }
            }

            return true;
        }

        Language::Definition ReadLanguageDefinition(const std::filesystem::path& path)
        {
            Language::Definition definition{
                .code = path.stem().string(),
                .displayName = path.stem().string()
            };
            std::unordered_map<std::string, std::string> ignored;
            LoadLanguageFile(path, ignored, &definition);
            return definition;
        }
    }

    bool Language::Load(std::string_view languageCode)
    {
        strings.clear();
        fallbackStrings.clear();
        currentDefinition = {};
        fallbackDefinition = { .code = "en", .displayName = "en" };

        currentLanguage = languageCode.empty() ? "en" : std::string(languageCode);
        REX::INFO("{}", "Loading language: " + currentLanguage);

        const auto fallbackPath = ResolveLanguagePath("en");
        if (!LoadLanguageFile(fallbackPath, fallbackStrings, &fallbackDefinition)) {
            REX::WARN("{}", "Failed to load English fallback language file");
        }

        const auto requestedPath = ResolveLanguagePath(currentLanguage);
        if (LoadLanguageFile(requestedPath, strings, &currentDefinition)) {
            REX::INFO("{}", "Loaded language file for: " + currentLanguage);
            return true;
        }

        strings = fallbackStrings;
        currentLanguage = "en";
        currentDefinition = fallbackDefinition;
        REX::WARN("{}", "Requested language file unavailable, falling back to English");
        return !strings.empty();
    }

    std::string_view Language::Get(std::string_view section, std::string_view key)
    {
        const auto mapKey = std::string(section) + "." + std::string(key);
        const auto it = strings.find(mapKey);
        if (it != strings.end()) {
            return it->second;
        }

        const auto fallback = fallbackStrings.find(mapKey);
        if (fallback != fallbackStrings.end()) {
            return fallback->second;
        }

        static std::string empty{};
        return empty;
    }

    std::vector<Language::Definition> Language::ListAvailableLanguages()
    {
        std::vector<Definition> result;
        const auto directory = ResolveLanguageDirectory();
        if (!std::filesystem::exists(directory)) {
            result.push_back({ .code = "en", .displayName = "English" });
            return result;
        }

        for (const auto& entry : std::filesystem::directory_iterator(directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto ext = entry.path().extension().string();
            if (_stricmp(ext.c_str(), ".ini") != 0) {
                continue;
            }

            result.push_back(ReadLanguageDefinition(entry.path()));
        }

        if (result.empty()) {
            result.push_back({ .code = "en", .displayName = "English" });
        }

        std::sort(result.begin(), result.end(), [](const Definition& left, const Definition& right) {
            return _stricmp(left.code.c_str(), right.code.c_str()) < 0;
        });
        result.erase(std::unique(result.begin(), result.end(), [](const Definition& left, const Definition& right) {
            return _stricmp(left.code.c_str(), right.code.c_str()) == 0;
        }), result.end());
        return result;
    }

    std::string Language::GetCurrentLanguageCode()
    {
        return currentLanguage;
    }

    std::vector<std::string> Language::GetActiveFontFiles()
    {
        if (!currentDefinition.fontFiles.empty()) {
            return currentDefinition.fontFiles;
        }

        return fallbackDefinition.fontFiles;
    }

    std::vector<std::string> Language::GetActiveGlyphRanges()
    {
        if (!currentDefinition.glyphRanges.empty()) {
            return currentDefinition.glyphRanges;
        }

        return fallbackDefinition.glyphRanges;
    }

    std::vector<std::string_view> Language::GetActiveGlyphSamples()
    {
        std::vector<std::string_view> result;
        result.reserve(fallbackStrings.size() + strings.size());

        for (const auto& [key, value] : fallbackStrings) {
            if (!value.empty()) {
                result.push_back(value);
            }
        }

        for (const auto& [key, value] : strings) {
            if (!value.empty()) {
                result.push_back(value);
            }
        }

        if (!currentDefinition.displayName.empty()) {
            result.push_back(currentDefinition.displayName);
        }

        return result;
    }
}
