#pragma once

#include "Core/RecordDetails.h"
#include <algorithm>
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
        static constexpr std::size_t Capacity = 16;

        std::shared_ptr<const RecordDetails> Request(DetailKey key, Milliseconds now)
        {
            std::erase_if(entries, [&](const Entry& entry) {
                return entry.key.session != key.session || entry.key.catalogGeneration != key.catalogGeneration;
            });
            auto found = Find(key);
            if (found == entries.end()) {
                if (entries.size() == Capacity) entries.erase(std::ranges::min_element(entries, {}, &Entry::requestedAt));
                entries.push_back({ key, ++revision, now, now, {} });
                found = std::prev(entries.end());
            }
            found->requestedAt = now;
            return found->current;
        }
        std::optional<Ticket> Begin(std::uint64_t session, std::uint64_t catalogGeneration, bool catalogReady, Milliseconds now)
        {
            // Session zero is the initial main menu, where the catalog can already
            // be ready. Details require the current session, not a loaded game.
            if (inFlight || !catalogReady) return {};
            Entry* next{};
            for (auto& entry : entries) {
                if (!entry.key.formID || entry.key.session != session || entry.key.catalogGeneration != catalogGeneration ||
                    now < entry.refreshAt || now - entry.requestedAt > RequestLease) continue;
                if (!next || entry.refreshAt < next->refreshAt) next = &entry;
            }
            if (next) inFlight = Ticket{ next->key, next->revision };
            return inFlight;
        }
        bool Finish(Ticket ticket, std::shared_ptr<const RecordDetails> result, Milliseconds now)
        {
            if (!inFlight || *inFlight != ticket) return false;
            inFlight.reset();
            const auto found = Find(ticket.key);
            if (found == entries.end() || found->revision != ticket.revision) return false;
            found->refreshAt = now + RefreshInterval;
            if (!result || result->key != ticket.key) return false;
            found->current = std::move(result);
            return true;
        }
        void Clear()
        {
            entries.clear();
            inFlight.reset();
            ++revision;
        }
        std::size_t RetainedCount() const { return entries.size(); }

    private:
        struct Entry
        {
            DetailKey key;
            std::uint64_t revision;
            Milliseconds requestedAt;
            Milliseconds refreshAt;
            std::shared_ptr<const RecordDetails> current;
        };
        std::vector<Entry> entries;
        std::uint64_t revision{};
        std::optional<Ticket> inFlight;
        std::vector<Entry>::iterator Find(DetailKey key) { return std::ranges::find(entries, key, &Entry::key); }
    };
}
