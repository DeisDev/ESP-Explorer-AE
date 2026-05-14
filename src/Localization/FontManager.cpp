#include "Localization/FontManager.h"

#include "Localization/Language.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace ESPExplorerAE
{
    namespace
    {
        std::string ToLower(std::string_view value)
        {
            std::string result(value);
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return result;
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

            ImFontConfig config{};
            config.MergeMode = mergeMode;
            config.PixelSnapH = true;
            return atlas->AddFontFromFileTTF(fontPath.string().c_str(), fontSize, &config, glyphRanges);
        }

        std::vector<std::string> GetDefaultFontFiles()
        {
            return {
                "ShareTechMono-Regular.ttf",
                "NotoSans-Regular.ttf",
                "NotoSansJP-Regular.ttf",
                "NotoSansSC-Regular.ttf",
                "NotoSansKR-Regular.ttf"
            };
        }

        void AppendUniqueFontFile(std::vector<std::string>& files, std::string_view file)
        {
            if (file.empty()) {
                return;
            }

            const std::string normalized = ToLower(file);
            const auto it = std::find_if(files.begin(), files.end(), [&](const std::string& existing) {
                return ToLower(existing) == normalized;
            });
            if (it == files.end()) {
                files.emplace_back(file);
            }
        }

        void AddConfiguredRange(ImFontGlyphRangesBuilder& builder, ImFontAtlas* atlas, std::string_view rangeName)
        {
            const std::string normalized = ToLower(rangeName);

            if (normalized == "default") {
                builder.AddRanges(atlas->GetGlyphRangesDefault());
            } else if (normalized == "cyrillic") {
                builder.AddRanges(atlas->GetGlyphRangesCyrillic());
            } else if (normalized == "japanese") {
                builder.AddRanges(atlas->GetGlyphRangesJapanese());
            } else if (normalized == "korean") {
                builder.AddRanges(atlas->GetGlyphRangesKorean());
            } else if (normalized == "chinese") {
                builder.AddRanges(atlas->GetGlyphRangesChineseSimplifiedCommon());
            } else if (normalized == "chinese-full") {
                builder.AddRanges(atlas->GetGlyphRangesChineseFull());
            } else if (normalized == "thai") {
                builder.AddRanges(atlas->GetGlyphRangesThai());
            } else if (normalized == "vietnamese") {
                builder.AddRanges(atlas->GetGlyphRangesVietnamese());
            }
        }

        ImVector<ImWchar> persistedGlyphRanges{};

        void RebuildGlyphRanges(ImFontAtlas* atlas)
        {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(atlas->GetGlyphRangesDefault());

            for (const auto& configuredRange : Language::GetActiveGlyphRanges()) {
                AddConfiguredRange(builder, atlas, configuredRange);
            }

            for (const auto sample : Language::GetActiveGlyphSamples()) {
                builder.AddText(sample.data());
            }

            persistedGlyphRanges.clear();
            builder.BuildRanges(&persistedGlyphRanges);
        }

        ImFont* BuildOneSize(
            ImFontAtlas* atlas,
            float fontSize,
            const std::filesystem::path& fontsDir)
        {
            std::vector<std::string> fontFiles = Language::GetActiveFontFiles();
            for (const auto& defaultFont : GetDefaultFontFiles()) {
                AppendUniqueFontFile(fontFiles, defaultFont);
            }

            RebuildGlyphRanges(atlas);

            ImFont* font = nullptr;

            for (const auto& fontFile : fontFiles) {
                const std::filesystem::path requestedPath(fontFile);
                const auto resolvedPath = requestedPath.is_absolute() ? requestedPath : (fontsDir / requestedPath);
                ImFont* loaded = AddFont(atlas, resolvedPath, fontSize, persistedGlyphRanges.Data, font != nullptr);
                if (!font && loaded) {
                    font = loaded;
                }
            }

            if (!font) {
                font = atlas->AddFontDefault();
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
            REX::WARN("{}", "Skipped font build because the atlas is unavailable or locked");
            return false;
        }

        currentLanguageCode = std::string(languageCode);

        io.Fonts->Clear();
        for (int i = 0; i < kPresetCount; ++i) {
            fonts[i] = nullptr;
        }

        const auto fontsDir = ResolveFontsDirectory();
        fonts[currentSizeIndex] = BuildOneSize(io.Fonts, kPresetSizes[currentSizeIndex], fontsDir);
        if (fonts[currentSizeIndex]) {
            REX::INFO("{}", "Built font atlas size index " + std::to_string(currentSizeIndex) + " for language " + currentLanguageCode);
        } else {
            REX::WARN("{}", "Failed to build font atlas for language " + currentLanguageCode);
        }
        return fonts[currentSizeIndex] != nullptr;
    }

    bool FontManager::EnsureCurrentFontBuilt()
    {
        if (fonts[currentSizeIndex]) {
            return false;
        }

        pendingLanguageCode = currentLanguageCode.empty() ? pendingLanguageCode : currentLanguageCode;
        if (pendingLanguageCode.empty()) {
            pendingLanguageCode = "en";
        }
        pendingRebuild = true;
        return true;
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
            if (currentSizeIndex != index) {
                REX::DEBUG("{}", "Font size index changed to " + std::to_string(index));
            }
            currentSizeIndex = index;
            EnsureCurrentFontBuilt();
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
        REX::INFO("{}", "Requested font rebuild for language " + pendingLanguageCode);
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
