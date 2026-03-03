#pragma once

#include "pch.h"

namespace ESPExplorerAE
{
    class FontManager
    {
    public:
        static bool Build(float fontSize, std::string_view languageCode);
        static void RequestRebuild(float fontSize, std::string_view languageCode);
        static bool HasPendingRebuild();
        static bool ProcessPendingRebuild();

    private:
        static std::filesystem::path ResolveFontsDirectory();

        static inline bool pendingRebuild{ false };
        static inline float pendingFontSize{ 16.0f };
        static inline std::string pendingLanguageCode{};
    };
}
