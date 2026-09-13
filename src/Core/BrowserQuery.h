#pragma once

#include "Core/CatalogQuery.h"
#include "Core/Profiling.h"

namespace ESPExplorerAE
{
    class BrowserQuery
    {
    public:
        const CatalogResult& Update(std::shared_ptr<const CatalogSnapshot> snapshot, CatalogQuery query,
            std::span<const AdvancedFilterRule> rules, std::uint64_t filterRevision)
        {
            const Key next{ snapshot ? snapshot->generation : 0, filterRevision, std::move(query) };
            if (key && *key == next) return result;
            const ProfileScope profileScope(ProfileMetric::CatalogQuery);
            if (!key || key->filters != filterRevision) prepared = PreparedRecordFilters(rules);
            result = QueryCatalog(std::move(snapshot), next.query, prepared, ++revision);
            key = next;
            return result;
        }
        std::size_t IndexBytes() const { return result.order.capacity() * sizeof(RecordIndex); }
        const CatalogQuery* Criteria() const { return key ? &key->query : nullptr; }
        void Clear() { key.reset(); result = {}; }

    private:
        struct Key
        {
            std::uint64_t generation;
            std::uint64_t filters;
            CatalogQuery query;
            bool operator==(const Key&) const = default;
        };
        std::optional<Key> key;
        PreparedRecordFilters prepared;
        CatalogResult result;
        std::uint64_t revision{};
    };
}
