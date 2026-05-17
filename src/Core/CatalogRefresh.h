#pragma once

#include <cstdint>
#include <optional>

namespace ESPExplorerAE
{
    // The owner synchronizes lifecycle requests and task completion. Ticket
    // identity prevents a pre-transition capture from publishing into a new load.
    class CatalogRefresh
    {
    public:
        void SetAvailable(bool value, bool waitForWorld = false)
        {
            if (value == available && waitForWorld == needsWorld) return;
            available = value;
            needsWorld = waitForWorld;
            ++requested;
            if (!value) completed = requested;
        }
        void Request() { ++requested; }
        std::optional<std::uint64_t> Begin(bool worldReady)
        {
            if (busy || !available || (needsWorld && !worldReady) || requested == completed) return {};
            busy = true;
            return requested;
        }
        bool Finish(std::uint64_t ticket, bool success)
        {
            busy = false;
            if (!available || ticket != requested || !success) return false;
            completed = ticket;
            return true;
        }

    private:
        std::uint64_t requested{};
        std::uint64_t completed{};
        bool available{};
        bool needsWorld{};
        bool busy{};
    };
}
