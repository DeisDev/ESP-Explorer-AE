#pragma once

#include "Core/InventorySnapshot.h"
#include <optional>

namespace ESPExplorerAE
{
    class InventoryRefresh
    {
    public:
        using Milliseconds = std::int64_t;
        struct Ticket { std::uint64_t revision; std::uint64_t session; friend bool operator==(const Ticket&, const Ticket&) = default; };

        std::shared_ptr<const InventorySnapshot> Request(std::uint64_t session, Milliseconds now, bool force = false)
        {
            if (session != requestedSession) { Clear(); requestedSession = session; }
            leaseUntil = now + 250;
            if (force) Invalidate();
            return published;
        }
        std::optional<Ticket> Begin(std::uint64_t session, bool ready, Milliseconds now)
        {
            if (!ready || !session || session != requestedSession || !Requested(now) || now < nextCapture || inFlight) return {};
            inFlight = Ticket{ revision, session };
            return inFlight;
        }
        void Finish(Ticket ticket, std::shared_ptr<const InventorySnapshot> value, Milliseconds now)
        {
            if (!inFlight || *inFlight != ticket) return;
            inFlight.reset();
            if (ticket.revision != revision || ticket.session != requestedSession) return;
            nextCapture = now + (value && value->ready ? 500 : 2000);
            if (value && value->session == ticket.session) published = std::move(value);
        }
        void Clear()
        {
            ++revision;
            requestedSession = 0;
            leaseUntil = 0;
            nextCapture = 0;
            published.reset();
        }
        bool Requested(Milliseconds now) const { return requestedSession && now <= leaseUntil; }
        bool HasSnapshot() const { return published != nullptr; }
        void Invalidate() { ++revision; nextCapture = 0; }

    private:
        std::uint64_t revision{};
        std::uint64_t requestedSession{};
        Milliseconds leaseUntil{};
        Milliseconds nextCapture{};
        std::optional<Ticket> inFlight;
        std::shared_ptr<const InventorySnapshot> published;
    };
}
