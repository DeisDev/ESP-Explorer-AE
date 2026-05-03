#pragma once

#include "Data/DataManager.h"

#include <functional>
#include <vector>

namespace ESPExplorerAE
{
    struct RecordFilterCache
    {
        const std::vector<FormEntry>* source{ nullptr };
        std::size_t sourceSize{ 0 };
        bool showPlayable{ true };
        bool showNonPlayable{ true };
        bool showNamed{ true };
        bool showUnnamed{ true };
        bool showDeleted{ true };
        std::uint64_t advancedFilterRevision{ 0 };
        std::vector<FormEntry> filtered{};

        using FilterFn = std::function<std::vector<FormEntry>(const std::vector<FormEntry>&)>;

        static const std::vector<FormEntry>& GetFiltered(
            RecordFilterCache& cache,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            std::uint64_t advancedFilterRevision,
            const FilterFn& filterEntries)
        {
            const bool needsRebuild =
                cache.source != &source ||
                cache.sourceSize != source.size() ||
                cache.showPlayable != showPlayable ||
                cache.showNonPlayable != showNonPlayable ||
                cache.showNamed != showNamed ||
                cache.showUnnamed != showUnnamed ||
                cache.showDeleted != showDeleted ||
                cache.advancedFilterRevision != advancedFilterRevision;

            if (needsRebuild) {
                cache.filtered = filterEntries(source);
                cache.source = &source;
                cache.sourceSize = source.size();
                cache.showPlayable = showPlayable;
                cache.showNonPlayable = showNonPlayable;
                cache.showNamed = showNamed;
                cache.showUnnamed = showUnnamed;
                cache.showDeleted = showDeleted;
                cache.advancedFilterRevision = advancedFilterRevision;
            }

            return cache.filtered;
        }
    };
}
