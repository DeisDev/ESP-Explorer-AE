#pragma once

#include <array>
#include <cstdint>

namespace ESPExplorerAE
{
    using ExecutableVersion = std::array<std::uint16_t, 4>;

    struct RuntimeRequirement
    {
        ExecutableVersion game;
        ExecutableVersion extender;
    };

    // One Address Library/layout family, with a matching F4SE release for each
    // executable. Do not admit an unknown future patch through a version range.
    inline constexpr std::array<RuntimeRequirement, 3> RuntimeRequirements{
        RuntimeRequirement{ { 1, 11, 191, 0 }, { 0, 7, 7, 0 } },
        RuntimeRequirement{ { 1, 11, 221, 0 }, { 0, 7, 8, 0 } },
        RuntimeRequirement{ { 1, 11, 240, 0 }, { 0, 7, 9, 0 } }
    };

    constexpr bool MeetsRuntimeRequirement(ExecutableVersion game, ExecutableVersion extender)
    {
        for (const auto& requirement : RuntimeRequirements) {
            if (game == requirement.game) return extender >= requirement.extender;
        }
        return false;
    }
}
