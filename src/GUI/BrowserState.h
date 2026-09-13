#pragma once

#include "GUI/Widgets/RecordFiltersWidget.h"

#include "Core/Actions.h"
#include "Core/BrowserQuery.h"
#include "GUI/Widgets/FormTable.h"

#include <array>

namespace ESPExplorerAE
{
    struct SharedBrowserFilters
    {
        bool showPlayableRecords{ true };
        bool showNonPlayableRecords{};
        bool showNamedRecords{ true };
        bool showUnnamedRecords{};
        bool showDeletedRecords{};
        std::vector<AdvancedFilterRule> advancedRecordFilters;
        std::unordered_set<std::string> hiddenPlugins;
        std::uint64_t advancedRecordFilterRevision{ 1 };
    };

    struct BrowserCategoryState
    {
        FormTableState table;
        BrowserQuery query;
        std::unordered_map<std::uint32_t, int> contextQuantities;
    };

    struct BrowserState
    {
        AdvancedFilterEditorState filterEditor;
        std::array<char, 1025> searchBuffer{};
        std::string search;
        bool structuredSearch{};
        RecordScope scope;
        bool focusPending{};
        std::unordered_map<std::string, BrowserCategoryState> categories;
        std::string activeCategory;
        std::string requestedCategory;
        ActionAdmission admission{ ActionAdmission::Accepted };

        void ResetSession()
        {
            filterEditor = {};
            admission = ActionAdmission::Accepted;
            for (auto& [id, rows] : categories) {
                rows.table.selection.Clear();
                rows.table.orderRevision.reset();
                rows.query.Clear();
                rows.contextQuantities.clear();
            }
        }
    };

    struct BrowserView
    {
        std::shared_ptr<const CatalogSnapshot> catalog;
        SharedBrowserFilters& filters;
        std::unordered_set<std::uint32_t>& favorites;
        std::string_view pluginFilter;
        std::uint64_t session{};
        bool gameplayReady{};
        std::function<const char*(std::string_view, std::string_view, const char*)> localize;
        std::function<void()> drawPluginFilterStatus;
        bool autoFocusSearch{};
        std::uint32_t equipWeaponAmmoCount{};
        MultiCopyFormat copyFormat{ MultiCopyFormat::Lines };
        bool doubleClickGameplayAction{};
        bool compactTableDensity{};
        bool componentSubstitution{true};
    };

    struct BrowserRequests
    {
        struct Action { ActionRequest request; bool confirm{}; std::string targetName; };
        bool filtersChanged{};
        std::vector<Action> actions;
        struct Grant { std::uint64_t session; std::vector<std::uint32_t> forms; };
        std::vector<Grant> grants;
        std::vector<std::uint32_t> recentSelections;
        std::vector<std::uint32_t> inspections;
        std::vector<std::uint32_t> pins;
        std::vector<std::uint32_t> collections;
        std::vector<std::uint32_t> basket;
        std::vector<std::uint32_t> pinA;
        std::vector<std::uint32_t> compareB;
        struct GlobalValue { std::uint64_t session; std::uint32_t formID; };
        std::vector<GlobalValue> globalValues;
    };
}
