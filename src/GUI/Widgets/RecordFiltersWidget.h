#pragma once

#include "Filters/AdvancedRecordFilters.h"

#include <functional>
#include <unordered_set>

namespace ESPExplorerAE
{
    struct RecordFilterState
    {
        bool& showNonPlayable;
        bool& showUnnamed;
        bool& showDeleted;
        std::vector<AdvancedFilterRule>& advancedRules;
        std::unordered_set<std::string>& hiddenPlugins;
    };

    class RecordFiltersWidget
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        static bool Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state);
        static void HandleMenuVisibilityChanged(bool visible);
    };
}
