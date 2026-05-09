#pragma once

#include "Config/Config.h"

#include <cstdint>
#include <span>
#include <string>

namespace ESPExplorerAE::FormatUtils
{
    std::string FormID(std::uint32_t formID);
    std::string MultiCopyList(std::span<const std::string> values, MultiCopyFormat format);
}
