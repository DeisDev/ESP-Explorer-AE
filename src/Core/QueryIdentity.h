#pragma once

#include <cstdint>
#include <string>

namespace ESPExplorerAE
{
    struct ResultRevision
    {
        std::uint64_t generation{ 0 };
        std::uint64_t revision{ 0 };
        bool operator==(const ResultRevision&) const = default;
    };

}
