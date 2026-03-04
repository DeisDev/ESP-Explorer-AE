#pragma once

#include "Data/DataManager.h"
#include "GUI/Widgets/ContextMenu.h"

#include <functional>

namespace ESPExplorerAE
{
    struct ItemSortState
    {
        int column{ 1 };
        bool ascending{ true };
    };

    struct ItemBrowserTabContext
    {
        std::string& selectedPluginFilter;
        std::string& itemSearch;
        char* itemSearchBuffer;
        std::size_t itemSearchBufferSize;
        bool& showPlayableRecords;
        bool& showNonPlayableRecords;
        bool& showNamedRecords;
        bool& showUnnamedRecords;
        bool& showDeletedRecords;
        bool* searchFocusPending;
        ItemSortState& itemSort;
        std::unordered_map<std::string, std::uint32_t>& selectedItemRows;
        std::unordered_set<std::uint32_t>& favoriteForms;

        std::function<const char*(std::string_view, std::string_view, const char*)> localize;
        std::function<void()> drawPluginFilterStatus;
        std::function<void()> persistListFilters;
        std::function<void()> persistFilterCheckboxes;
        std::function<void(const FormEntry&)> openItemGrantPopup;
        std::function<void(const std::vector<FormEntry>&)> openItemGrantPopupMultiple;
        std::function<const char*(std::uint32_t)> tryGetEditorID;
        ContextMenuCallbacks contextCallbacks{};
    };

    class ItemBrowserTab
    {
    public:
        static void Draw(const FormCache& cache, ItemBrowserTabContext& context);
    };
}
