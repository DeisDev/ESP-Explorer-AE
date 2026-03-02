#pragma once

#include "Data/DataManager.h"

#include <functional>

namespace ESPExplorerAE
{
    class NPCBrowserTab
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;
        using FilterEntriesFn = std::function<std::vector<FormEntry>(std::vector<FormEntry>)>;

        static void Draw(
            const FormCache& cache,
            char* searchBuffer,
            std::size_t searchBufferSize,
            std::string& searchText,
            bool& searchCaseSensitive,
            std::string_view selectedPluginFilter,
            bool& showPlayableRecords,
            bool& showNonPlayableRecords,
            bool& showNamedRecords,
            bool& showUnnamedRecords,
            bool& showDeletedRecords,
            std::unordered_set<std::uint32_t>& favoriteForms,
            const std::function<void()>& drawPluginFilterStatus,
            const std::function<void()>& persistListFilters,
            const FilterEntriesFn& filterEntries,
            const LocalizeFn& localize);
    };
}
