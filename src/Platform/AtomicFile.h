#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace ESPExplorerAE
{
    // Success means bytes were flushed and replaced in the same directory.
    // Failure retains the previous destination and returns an actionable error.
    bool WriteFileAtomically(const std::filesystem::path& path, std::string_view bytes, std::string& error) noexcept;
    // Writes a uniquely named, flushed backup beside the original. Never
    // replaces an existing backup or the original configuration.
    bool WriteConfigBackup(const std::filesystem::path& original, std::string_view bytes, std::filesystem::path& backup, std::string& error) noexcept;
}
