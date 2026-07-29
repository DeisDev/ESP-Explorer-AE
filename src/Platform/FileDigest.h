#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace ESPExplorerAE { std::optional<std::string> SHA256File(const std::filesystem::path& path); }
