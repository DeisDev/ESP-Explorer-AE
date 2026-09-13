#pragma once

#include "Core/OverlayPolicy.h"
#include <algorithm>
#include <chrono>
#include <optional>

namespace ESPExplorerAE
{
    // Owned by the Present thread for the entire launch, including renderer
    // retries and save transitions. Readiness starts the timer, not DLL load.
    class ToggleHint
    {
    public:
        using Clock = std::chrono::steady_clock;

        float Update(const OverlayFacts& facts, bool catalogReady, Clock::time_point now)
        {
            if (finished) return 0.0f;
            if (started && now - *started >= std::chrono::seconds(8)) {
                finished = true;
                return 0.0f;
            }

            auto opening = facts;
            opening.visible = true;
            const bool canOpen = DecideOverlay(opening, {}).render;
            if (canOpen && facts.visible) {
                finished = true;
                return 0.0f;
            }
            if (!canOpen || !catalogReady || (!facts.mainMenu && !facts.worldReady)) return 0.0f;

            if (!started) started = now;
            const auto remaining = std::chrono::duration<float>(std::chrono::seconds(8) - (now - *started)).count();
            return std::clamp(remaining, 0.0f, 1.0f);
        }

    private:
        std::optional<Clock::time_point> started;
        bool finished{};
    };
}
