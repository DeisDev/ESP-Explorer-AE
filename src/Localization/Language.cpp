#include "Localization/Language.h"

namespace ESPExplorerAE
{
    static std::filesystem::path ResolveLanguageDirectory()
    {
        const auto runtimePath = std::filesystem::path("Data/Interface/ESPExplorerAE/lang");
        if (std::filesystem::exists(runtimePath)) {
            return runtimePath;
        }

        return std::filesystem::path("dist/lang");
    }

    static std::filesystem::path ResolveLanguagePath(std::string_view code)
    {
        const auto basePath = ResolveLanguageDirectory();
        return basePath / (std::string(code) + ".ini");
    }

    static bool LoadIntoMap(const std::filesystem::path& path, std::unordered_map<std::string, std::string>& out)
    {
        CSimpleIniA ini;
        ini.SetUnicode();

        if (ini.LoadFile(path.string().c_str()) < 0) {
            return false;
        }

        CSimpleIniA::TNamesDepend sections;
        ini.GetAllSections(sections);

        for (const auto& section : sections) {
            CSimpleIniA::TNamesDepend keys;
            ini.GetAllKeys(section.pItem, keys);

            for (const auto& key : keys) {
                const auto value = ini.GetValue(section.pItem, key.pItem, "");
                out[std::string(section.pItem) + "." + std::string(key.pItem)] = value;
            }
        }

        return true;
    }

    bool Language::Load(std::string_view languageCode)
    {
        strings.clear();
        fallbackStrings.clear();

        currentLanguage = languageCode.empty() ? "en" : std::string(languageCode);

        const auto fallbackPath = ResolveLanguagePath("en");
        LoadIntoMap(fallbackPath, fallbackStrings);

        const auto requestedPath = ResolveLanguagePath(currentLanguage);
        if (LoadIntoMap(requestedPath, strings)) {
            return true;
        }

        strings = fallbackStrings;
        currentLanguage = "en";
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

    std::vector<std::string> Language::ListAvailableLanguages()
    {
        std::vector<std::string> result;
        const auto directory = ResolveLanguageDirectory();
        if (!std::filesystem::exists(directory)) {
            result.push_back("en");
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

            result.push_back(entry.path().stem().string());
        }

        if (result.empty()) {
            result.push_back("en");
        }

        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    std::string Language::GetCurrentLanguageCode()
    {
        return currentLanguage;
    }
}
