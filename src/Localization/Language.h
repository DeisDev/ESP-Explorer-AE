#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    class Language
    {
    public:
        struct Definition
        {
            std::string code;
            std::string displayName;
            std::vector<std::string> fontFiles;
            std::vector<std::string> glyphRanges;
        };

        static bool Load(std::string_view languageCode);
        static std::string_view Get(std::string_view section, std::string_view key);
        static std::vector<Definition> ListAvailableLanguages();
        static std::string GetCurrentLanguageCode();
        static std::vector<std::string> GetActiveFontFiles();
        static std::vector<std::string> GetActiveGlyphRanges();
        static std::vector<std::string_view> GetActiveGlyphSamples();

    private:
        static inline std::unordered_map<std::string, std::string> strings{};
        static inline std::unordered_map<std::string, std::string> fallbackStrings{};
        static inline std::string currentLanguage{ "en" };
        static inline Definition currentDefinition{ .code = "en", .displayName = "en" };
        static inline Definition fallbackDefinition{ .code = "en", .displayName = "en" };
    };
}
