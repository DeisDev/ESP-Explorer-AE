#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ESPExplorerAE::FormatUtils
{
    std::string FormID(std::uint32_t formID);
    std::string ParenthesizedList(std::span<const std::string> values);
}
