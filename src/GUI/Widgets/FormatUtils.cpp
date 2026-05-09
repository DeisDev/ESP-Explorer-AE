#include "GUI/Widgets/FormatUtils.h"

#include <cstdio>

namespace ESPExplorerAE::FormatUtils
{
    std::string FormID(std::uint32_t formID)
    {
        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "%08X", formID);
        return buffer;
    }

    std::string ParenthesizedList(std::span<const std::string> values)
    {
        std::string text{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                text += ", ";
            }
            text += "(";
            text += values[i];
            text += ")";
        }
        return text;
    }
}
