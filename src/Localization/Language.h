#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    class Language
    {
    public:
        static bool Load(std::string_view languageCode);
        static std::string_view Get(std::string_view section, std::string_view key);
        static std::vector<std::string> ListAvailableLanguages();
        static std::string GetCurrentLanguageCode();

    private:
        static inline std::unordered_map<std::string, std::string> strings{};
        static inline std::unordered_map<std::string, std::string> fallbackStrings{};
        static inline std::string currentLanguage{ "en" };
    };
}
