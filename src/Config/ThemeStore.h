#pragma once

#include "Core/Theme.h"
#include <filesystem>
#include <memory>
#include <functional>

namespace ESPExplorerAE
{
    class ThemeStore
    {
    public:
        using Diagnostic = std::function<void(std::string)>;
        static std::filesystem::path ResolveThemesDirectory();
        static std::vector<ThemePreset> ReadDirectory(const std::filesystem::path& directory, const Diagnostic& diagnostic = {});
        static void ReloadAvailableThemes(const Diagnostic& diagnostic = {});
        static std::shared_ptr<const std::vector<ThemePreset>> Snapshot(const Diagnostic& diagnostic = {});
    };
}
