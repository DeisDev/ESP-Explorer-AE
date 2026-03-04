#pragma once

#include "Data/DataManager.h"
#include "GUI/Widgets/ContextMenu.h"

#include <deque>
#include <functional>

namespace ESPExplorerAE
{
    struct PluginBrowserTabContext
    {
        std::string& pluginSearch;
        char* pluginSearchBuffer;
        std::size_t pluginSearchBufferSize;
        std::string& selectedPluginFilter;

        bool& showPlayableRecords;
        bool& showNonPlayableRecords;
        bool& showNamedRecords;
        bool& showUnnamedRecords;
        bool& showDeletedRecords;
        bool& showUnknownCategories;
        bool& pluginGlobalSearchMode;
        int& equipWeaponAmmoCount;
        bool* searchFocusPending;

        std::unordered_set<std::uint32_t>& favoriteForms;
        std::uint32_t& selectedPluginTreeRecordFormID;
        std::unordered_set<std::uint32_t>& selectedPluginTreeRecordFormIDs;
        std::uint32_t& pluginTreeLastClickedFormID;
        std::deque<std::uint32_t>& recentPluginRecordFormIDs;

        std::uint64_t& pluginBrowserCacheVersion;
        std::string& pluginBrowserCacheSearch;
        std::string& pluginBrowserCacheSelectedPlugin;
        bool& pluginBrowserCacheShowPlayable;
        bool& pluginBrowserCacheShowNonPlayable;
        bool& pluginBrowserCacheShowNamed;
        bool& pluginBrowserCacheShowUnnamed;
        bool& pluginBrowserCacheShowDeleted;
        bool& pluginBrowserCacheShowUnknown;
        bool& pluginBrowserCacheGlobalSearchMode;
        std::unordered_map<std::string, std::unordered_map<std::string, std::vector<const FormEntry*>>>& pluginBrowserGroupedRecordsCache;
        std::vector<std::string>& pluginBrowserOrderedPluginsCache;
        std::vector<const FormEntry*>& pluginBrowserGlobalSearchResultsCache;

        std::function<const char*(std::string_view, std::string_view, const char*)> localize;
        std::function<void()> persistListFilters;
        std::function<void()> persistFilterCheckboxes;
        std::function<void(const FormEntry&)> openItemGrantPopup;
        std::function<void(const std::vector<FormEntry>&)> openItemGrantPopupMultiple;
        std::function<void(std::uint32_t)> openGlobalValuePopup;
        std::function<void(std::string, std::string, std::function<void()>)> requestActionConfirmation;
    };

    class PluginBrowserTab
    {
    public:
        static void Draw(const std::vector<PluginInfo>& plugins, const FormCache& cache, std::uint64_t dataVersion, PluginBrowserTabContext& context);
    };
}
