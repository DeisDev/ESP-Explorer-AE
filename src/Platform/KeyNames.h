#pragma once

#include <cstdint>
#include <string>

namespace ESPExplorerAE
{
    // Local keyboard-layout names, encoded for ImGui's UTF-8 text inputs.
    std::string KeyboardKeyName(std::uint32_t key);
}
