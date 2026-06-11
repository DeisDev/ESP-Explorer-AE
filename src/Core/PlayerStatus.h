#pragma once

#include <cstdint>
#include <optional>

namespace ESPExplorerAE
{
    struct PlayerStatus
    {
        std::uint64_t session{};
        bool ready{};
        std::int32_t level{};
        std::int64_t caps{};
        float health{};
        float actionPoints{};
    };

    // Demand lasts briefly across frames. Session/world changes invalidate both
    // published values and in-flight reads without retaining game objects.
    class PlayerStatusState
    {
    public:
        using Milliseconds = std::int64_t;
        struct Ticket { std::uint64_t session; std::uint64_t epoch; };

        PlayerStatus Request(std::uint64_t session, Milliseconds now)
        {
            if (requestedSession != session) {
                Reset();
                requestedSession = session;
                value.session = session;
            }
            if (now > requestedUntil) value = { .session = session };
            requestedUntil = now + 250;
            return value;
        }

        void Reset()
        {
            ++epoch;
            requestedSession = 0;
            requestedUntil = 0;
            inFlight = false;
            value = {};
        }

        std::optional<Ticket> Begin(std::uint64_t session, bool ready, Milliseconds now)
        {
            if (!session || requestedSession != session || !ready || now > requestedUntil) {
                value = { .session = requestedSession };
                if (inFlight) { ++epoch; inFlight = false; }
                return {};
            }
            if (inFlight) return {};
            inFlight = true;
            return Ticket{ session, epoch };
        }

        bool Finish(Ticket ticket, PlayerStatus next, Milliseconds now)
        {
            if (!inFlight || ticket.epoch != epoch || ticket.session != requestedSession) return false;
            inFlight = false;
            if (next.session != requestedSession || now > requestedUntil) value = { .session = requestedSession };
            else value = next;
            return true;
        }

    private:
        PlayerStatus value;
        std::uint64_t requestedSession{};
        std::uint64_t epoch{};
        Milliseconds requestedUntil{};
        bool inFlight{};
    };
}
