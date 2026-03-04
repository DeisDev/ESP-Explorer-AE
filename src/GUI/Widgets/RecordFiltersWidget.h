#pragma once

#include <functional>

namespace ESPExplorerAE
{
    struct RecordFilterState
    {
        bool& showNonPlayable;
        bool& showUnnamed;
        bool& showDeleted;
    };

    class RecordFiltersWidget
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        static bool Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state);
    };
}
