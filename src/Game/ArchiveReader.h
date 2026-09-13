#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ESPExplorerAE
{
    class ArchiveReader
    {
    public:
        // Game-task capture of the engine's registered archive collection.
        // A missing result means unavailable, never an empty archive list.
        static std::optional<std::vector<std::string>> Capture();
    };
}
