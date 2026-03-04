#include "Localization/FontManager.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <fstream>

namespace ESPExplorerAE
{
    namespace
    {
        bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), value.begin());
        }

        ImFont* AddFont(
            ImFontAtlas* atlas,
            const std::filesystem::path& fontPath,
            float fontSize,
            const ImWchar* glyphRanges,
            bool mergeMode)
        {
            if (!atlas || atlas->Locked) {
                return nullptr;
            }

            if (mergeMode && atlas->Fonts.empty()) {
                return nullptr;
            }

            if (!std::filesystem::exists(fontPath)) {
                return nullptr;
            }

            std::ifstream stream(fontPath, std::ios::binary);
            if (!stream.good()) {
                return nullptr;
            }

            ImFontConfig config{};
            config.MergeMode = mergeMode;
            config.PixelSnapH = true;
            return atlas->AddFontFromFileTTF(fontPath.string().c_str(), fontSize, &config, glyphRanges);
        }

        ImFont* BuildOneSize(
            ImFontAtlas* atlas,
            float fontSize,
            std::string_view languageCode,
            const std::filesystem::path& fontsDir)
        {
            const auto shareTechMono = fontsDir / "ShareTechMono-Regular.ttf";
            const auto notoSans = fontsDir / "NotoSans-Regular.ttf";
            const auto notoSansJP = fontsDir / "NotoSansJP-Regular.ttf";
            const auto notoSansSC = fontsDir / "NotoSansSC-Regular.ttf";
            const auto notoSansKR = fontsDir / "NotoSansKR-Regular.ttf";

            const std::string language = std::string(languageCode);
            const bool isEnglish = StartsWith(language, "en");
            const bool isRussian = StartsWith(language, "ru") || StartsWith(language, "uk") || StartsWith(language, "be") || StartsWith(language, "bg") || StartsWith(language, "sr");
            const bool isJapanese = StartsWith(language, "ja");
            const bool isChinese = StartsWith(language, "zh");
            const bool isKorean = StartsWith(language, "ko");

            ImFont* font = nullptr;

            if (isEnglish) {
                font = AddFont(atlas, shareTechMono, fontSize, atlas->GetGlyphRangesDefault(), false);
                if (!font) {
                    font = AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), false);
                }
            } else if (isRussian) {
                font = AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesCyrillic(), false);
            } else if (isJapanese) {
                font = AddFont(atlas, notoSansJP, fontSize, atlas->GetGlyphRangesJapanese(), false);
            } else if (isChinese) {
                font = AddFont(atlas, notoSansSC, fontSize, atlas->GetGlyphRangesChineseFull(), false);
            } else if (isKorean) {
                font = AddFont(atlas, notoSansKR, fontSize, atlas->GetGlyphRangesKorean(), false);
            } else {
                font = AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), false);
            }

            if (!font) {
                font = AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), false);
            }

            if (!font) {
                font = atlas->AddFontDefault();
            }

            if (isEnglish) {
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesCyrillic(), true);
                AddFont(atlas, notoSansJP, fontSize, atlas->GetGlyphRangesJapanese(), true);
                AddFont(atlas, notoSansSC, fontSize, atlas->GetGlyphRangesChineseFull(), true);
                AddFont(atlas, notoSansKR, fontSize, atlas->GetGlyphRangesKorean(), true);
            } else if (isRussian) {
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), true);
            } else if (isJapanese) {
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), true);
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesCyrillic(), true);
            } else if (isChinese) {
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), true);
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesCyrillic(), true);
            } else if (isKorean) {
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesDefault(), true);
                AddFont(atlas, notoSans, fontSize, atlas->GetGlyphRangesCyrillic(), true);
            }

            return font;
        }
    }

    std::filesystem::path FontManager::ResolveFontsDirectory()
    {
        const auto runtimePath = std::filesystem::path("Data/Interface/ESPExplorerAE/fonts");
        if (std::filesystem::exists(runtimePath)) {
            return runtimePath;
        }

        return std::filesystem::path("dist/fonts");
    }

    bool FontManager::BuildAll(std::string_view languageCode)
    {
        auto& io = ImGui::GetIO();
        if (!io.Fonts || io.Fonts->Locked) {
            return false;
        }

        io.Fonts->Clear();
        for (int i = 0; i < kPresetCount; ++i) {
            fonts[i] = nullptr;
        }

        currentLanguageCode = std::string(languageCode);
        const auto fontsDir = ResolveFontsDirectory();

        bool anyBuilt = false;
        for (int i = 0; i < kPresetCount; ++i) {
            fonts[i] = BuildOneSize(io.Fonts, kPresetSizes[i], languageCode, fontsDir);
            if (fonts[i]) {
                anyBuilt = true;
            }
        }

        return anyBuilt;
    }

    ImFont* FontManager::GetFont(int sizeIndex)
    {
        if (sizeIndex < 0 || sizeIndex >= kPresetCount) {
            sizeIndex = kDefaultSizeIndex;
        }
        return fonts[sizeIndex];
    }

    ImFont* FontManager::GetCurrentFont()
    {
        return GetFont(currentSizeIndex);
    }

    int FontManager::GetCurrentSizeIndex()
    {
        return currentSizeIndex;
    }

    void FontManager::SetCurrentSizeIndex(int index)
    {
        if (index >= 0 && index < kPresetCount) {
            currentSizeIndex = index;
        }
    }

    int FontManager::FindClosestSizeIndex(float fontSize)
    {
        int best = kDefaultSizeIndex;
        float bestDiff = std::abs(fontSize - kPresetSizes[kDefaultSizeIndex]);
        for (int i = 0; i < kPresetCount; ++i) {
            const float diff = std::abs(fontSize - kPresetSizes[i]);
            if (diff < bestDiff) {
                bestDiff = diff;
                best = i;
            }
        }
        return best;
    }

    void FontManager::RequestLanguageRebuild(std::string_view languageCode)
    {
        pendingLanguageCode = std::string(languageCode);
        pendingRebuild = true;
    }

    bool FontManager::HasPendingRebuild()
    {
        return pendingRebuild;
    }

    bool FontManager::ProcessPendingRebuild()
    {
        if (!pendingRebuild) {
            return false;
        }

        pendingRebuild = false;
        return BuildAll(pendingLanguageCode);
    }
}
