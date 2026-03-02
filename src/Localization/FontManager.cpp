#include "Localization/FontManager.h"

#include <imgui.h>

#include <algorithm>

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
            if (!std::filesystem::exists(fontPath)) {
                return nullptr;
            }

            ImFontConfig config{};
            config.MergeMode = mergeMode;
            config.PixelSnapH = true;
            return atlas->AddFontFromFileTTF(fontPath.string().c_str(), fontSize, &config, glyphRanges);
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

    bool FontManager::Build(float fontSize, std::string_view languageCode)
    {
        auto& io = ImGui::GetIO();
        io.Fonts->Clear();

        const auto fontsDir = ResolveFontsDirectory();
        const auto shareTechMono = fontsDir / "ShareTechMono-Regular.ttf";
        const auto notoSans = fontsDir / "NotoSans-Regular.ttf";
        const auto notoSansJP = fontsDir / "NotoSansJP-Regular.ttf";
        const auto notoSansSC = fontsDir / "NotoSansSC-Regular.ttf";
        const auto notoSansKR = fontsDir / "NotoSansKR-Regular.ttf";

        ImFont* font = nullptr;

        const std::string language = std::string(languageCode);
        const bool isEnglish = StartsWith(language, "en");
        const bool isRussian = StartsWith(language, "ru") || StartsWith(language, "uk") || StartsWith(language, "be") || StartsWith(language, "bg") || StartsWith(language, "sr");
        const bool isJapanese = StartsWith(language, "ja");
        const bool isChinese = StartsWith(language, "zh");
        const bool isKorean = StartsWith(language, "ko");

        if (isEnglish) {
            font = AddFont(io.Fonts, shareTechMono, fontSize, io.Fonts->GetGlyphRangesDefault(), false);
            if (!font) {
                font = AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), false);
            }
        } else if (isRussian) {
            font = AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesCyrillic(), false);
        } else if (isJapanese) {
            font = AddFont(io.Fonts, notoSansJP, fontSize, io.Fonts->GetGlyphRangesJapanese(), false);
        } else if (isChinese) {
            font = AddFont(io.Fonts, notoSansSC, fontSize, io.Fonts->GetGlyphRangesChineseFull(), false);
        } else if (isKorean) {
            font = AddFont(io.Fonts, notoSansKR, fontSize, io.Fonts->GetGlyphRangesKorean(), false);
        } else {
            font = AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), false);
        }

        if (!font) {
            font = AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), false);
        }

        if (!font) {
            font = io.Fonts->AddFontDefault();
        }

        if (isEnglish) {
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesCyrillic(), true);
            AddFont(io.Fonts, notoSansJP, fontSize, io.Fonts->GetGlyphRangesJapanese(), true);
            AddFont(io.Fonts, notoSansSC, fontSize, io.Fonts->GetGlyphRangesChineseFull(), true);
            AddFont(io.Fonts, notoSansKR, fontSize, io.Fonts->GetGlyphRangesKorean(), true);
        } else if (isRussian) {
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), true);
        } else if (isJapanese) {
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), true);
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesCyrillic(), true);
        } else if (isChinese) {
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), true);
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesCyrillic(), true);
        } else if (isKorean) {
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesDefault(), true);
            AddFont(io.Fonts, notoSans, fontSize, io.Fonts->GetGlyphRangesCyrillic(), true);
        }

        io.Fonts->Build();
        return font != nullptr;
    }
}
