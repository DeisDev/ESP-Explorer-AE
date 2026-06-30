#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ESPExplorerAE
{
    struct LogSnapshot
    {
        std::vector<std::string> files;
        std::string selectedFile;
        std::vector<std::string> lines;
        std::uint64_t selectionRevision{};
    };

    struct LogRequests
    {
        std::optional<std::string> selectFile;
        bool refresh{}, openFile{}, openFolder{}, exportFile{};
    };
}
