#pragma once

#include <chrono>

namespace ESPExplorerAE
{
    class SaveSchedule
    {
    public:
        using Clock = std::chrono::steady_clock;

        void Request(Clock::time_point now)
        {
            dirty = true;
            deadline = now + std::chrono::milliseconds(300);
        }

        bool IsDirty() const { return dirty; }
        bool IsDue(Clock::time_point now) const { return dirty && now >= deadline; }

        void Complete(bool success, Clock::time_point now)
        {
            dirty = !success;
            // Failed storage should retry without writing/logging every frame.
            if (!success) {
                deadline = now + std::chrono::seconds(2);
            }
        }

    private:
        bool dirty{ false };
        Clock::time_point deadline{};
    };
}
