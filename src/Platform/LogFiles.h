#pragma once
#include <filesystem>
#include <string_view>
#include <Windows.h>

namespace ESPExplorerAE::LogFiles
{
    std::filesystem::path Documents();
    void Open(const std::filesystem::path& path);
    void Export(HWND owner, const std::filesystem::path& source, std::string_view logLabel, std::string_view allLabel);
}
