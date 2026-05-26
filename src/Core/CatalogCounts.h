#pragma once

#include "Core/CatalogQuery.h"
#include <map>
#include <initializer_list>

namespace ESPExplorerAE
{
    struct RecordCount
    {
        std::size_t total{};
        std::size_t filtered{};
    };

    struct CatalogCounts
    {
        std::map<std::string, RecordCount, std::less<>> types;
        RecordCount For(std::initializer_list<std::string_view> categories) const
        {
            RecordCount result;
            for (const auto category : categories) if (const auto found = types.find(category); found != types.end()) {
                result.total += found->second.total;
                result.filtered += found->second.filtered;
            }
            return result;
        }
    };

    inline CatalogCounts CountCatalog(const CatalogSnapshot& catalog, const CatalogQuery& query, const PreparedRecordFilters& filters)
    {
        CatalogCounts counts;
        for (const auto& record : catalog.records) {
            auto& type = counts.types[record.category];
            ++type.total;
            if (query.Matches(record, filters)) ++type.filtered;
        }
        return counts;
    }
}
