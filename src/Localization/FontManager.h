#pragma once

#include <filesystem>
#include <string>
#include "Core/FontSizes.h"

struct ImFont;

namespace ESPExplorerAE
{
    class FontManager
    {
    public:
        static constexpr auto kPresetSizes = FontSizes;
        static constexpr int kPresetCount = static_cast<int>(FontSizes.size());
        static constexpr int kDefaultSizeIndex = 4;

        static void ResetAtlasState();
        static bool BuildAll();
        static bool EnsureCurrentFontBuilt();

        static ImFont* GetFont(int sizeIndex);
        static ImFont* GetCurrentFont();
        static int GetCurrentSizeIndex();
        static void SetCurrentSizeIndex(int index);
        static int FindClosestSizeIndex(float fontSize);

        static void RequestLanguageRebuild();
        static bool HasPendingRebuild();
        static bool ProcessPendingRebuild();

    private:
        static std::filesystem::path ResolveFontsDirectory();

        static inline ImFont* fonts[kPresetCount]{ nullptr };
        static inline int currentSizeIndex{ kDefaultSizeIndex };
        static inline bool pendingRebuild{ false };
    };
}
