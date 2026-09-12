#pragma once

#include "Core/RecordDetails.h"
#include <memory>

namespace ESPExplorerAE
{
    class DetailRefresh
    {
    public:
        using Milliseconds = std::uint64_t;
        struct Ticket { DetailKey key; std::uint64_t revision; bool operator==(const Ticket&) const = default; };
        static constexpr Milliseconds RefreshInterval = 500;
        static constexpr Milliseconds RequestLease = 250;

        std::shared_ptr<const RecordDetails> Request(DetailKey key, Milliseconds now)
        {
            if (!requested || *requested != key) {
                requested = key;
                ++revision;
                current.reset();
                refreshAt = now;
            }
            requestedAt = now;
            return current;
        }
        std::optional<Ticket> Begin(std::uint64_t session, std::uint64_t catalogGeneration, bool catalogReady, Milliseconds now)
        {
            // Session zero is the initial main menu, where the catalog can already
            // be ready. Details require the current session, not a loaded game.
            if (inFlight || !requested || !catalogReady || !requested->formID || requested->session != session ||
                requested->catalogGeneration != catalogGeneration || now < refreshAt || now - requestedAt > RequestLease) return {};
            inFlight = Ticket{ *requested, revision };
            return inFlight;
        }
        bool Finish(Ticket ticket, std::shared_ptr<const RecordDetails> result, Milliseconds now)
        {
            if (!inFlight || *inFlight != ticket) return false;
            inFlight.reset();
            if (!requested || ticket.revision != revision || ticket.key != *requested) return false;
            refreshAt = now + RefreshInterval;
            if (!result || result->key != ticket.key) return false;
            current = std::move(result);
            return true;
        }
        void Clear()
        {
            requested.reset();
            current.reset();
            ++revision;
            refreshAt = 0;
        }

    private:
        std::optional<DetailKey> requested;
        std::shared_ptr<const RecordDetails> current;
        std::uint64_t revision{};
        Milliseconds requestedAt{};
        Milliseconds refreshAt{};
        std::optional<Ticket> inFlight;
    };
}
