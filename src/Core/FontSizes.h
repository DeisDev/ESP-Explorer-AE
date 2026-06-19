#pragma once

#include <array>
#include <cmath>

namespace ESPExplorerAE
{
    inline constexpr std::array FontSizes{ 12.0f, 14.0f, 16.0f, 18.0f, 20.0f, 22.0f, 24.0f };
    inline int ClosestFontSizeIndex(float size)
    {
        int best = 4;
        float difference = std::abs(size - FontSizes[best]);
        for (int index = 0; index < static_cast<int>(FontSizes.size()); ++index) {
            const auto candidate = std::abs(size - FontSizes[index]);
            if (candidate < difference) { difference = candidate; best = index; }
        }
        return best;
    }
}
