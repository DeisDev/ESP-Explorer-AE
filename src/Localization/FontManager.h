#pragma once

#include "pch.h"

struct ImFont;

namespace ESPExplorerAE
{
    class FontManager
    {
    public:
        static constexpr float kPresetSizes[] = { 12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f, 24.0f };
        static constexpr int kPresetCount = 7;
        static constexpr int kDefaultSizeIndex = 4;

        static bool BuildAll(std::string_view languageCode);
        static bool EnsureCurrentFontBuilt();

        static ImFont* GetFont(int sizeIndex);
        static ImFont* GetCurrentFont();
        static int GetCurrentSizeIndex();
        static void SetCurrentSizeIndex(int index);
        static int FindClosestSizeIndex(float fontSize);

        static void RequestLanguageRebuild(std::string_view languageCode);
        static bool HasPendingRebuild();
        static bool ProcessPendingRebuild();

    private:
        static std::filesystem::path ResolveFontsDirectory();

        static inline ImFont* fonts[kPresetCount]{ nullptr };
        static inline int currentSizeIndex{ kDefaultSizeIndex };
        static inline bool pendingRebuild{ false };
        static inline std::string pendingLanguageCode{};
        static inline std::string currentLanguageCode{};
    };
}
