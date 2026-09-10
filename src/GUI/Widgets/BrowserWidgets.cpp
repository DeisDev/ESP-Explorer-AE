#include "GUI/Widgets/BrowserWidgets.h"

#include "Core/RecordActions.h"
#include "GUI/Widgets/ActionFeedback.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE::BrowserWidgets
{
    void DrawControls(BrowserState& state, const BrowserView& view, BrowserRequests& requests,
        const char* id, const char* section, const char* searchFallback)
    {
        auto& filters = view.filters;
        if (RecordFiltersWidget::Draw(view.localize, id, RecordFilterState{
                .showNonPlayable = filters.showNonPlayableRecords, .showUnnamed = filters.showUnnamedRecords,
                .showDeleted = filters.showDeletedRecords, .advancedRules = filters.advancedRecordFilters,
                .hiddenPlugins = filters.hiddenPlugins }, state.filterEditor, view.catalog)) {
            ++filters.advancedRecordFilterRevision;
            requests.filtersChanged = true;
        }
        SearchBar::Draw(view.localize(section, "sSearch", searchFallback), state.searchBuffer.data(), state.searchBuffer.size(), state.search, &state.focusPending, id,
            view.localize("General", "sClearSearchButton", "X"));
        if (view.drawPluginFilterStatus) view.drawPluginFilterStatus();
        ActionFeedback::Draw(state.admission, view.localize);
        ImGui::Separator();
    }

    CatalogQuery MakeQuery(const BrowserState& state, const BrowserView& view, const BrowserCategoryState& rows, std::string type)
    {
        const auto& filters = view.filters;
        return { .type = std::move(type), .plugin = std::string(view.pluginFilter), .search = state.search,
            .showPlayable = filters.showPlayableRecords, .showNonPlayable = filters.showNonPlayableRecords,
            .showNamed = filters.showNamedRecords, .showUnnamed = filters.showUnnamedRecords, .showDeleted = filters.showDeletedRecords,
            .sortColumn = rows.table.sort.column, .ascending = rows.table.sort.ascending, .hiddenPlugins = filters.hiddenPlugins };
    }

    void DrawCategory(BrowserState& state, const BrowserView& view, BrowserRequests& requests,
        const char* type, const FormTableConfig& config, ActionKind primary, std::optional<ActionKind> secondary)
    {
        auto& rows = state.categories[type];
        const auto query = MakeQuery(state, view, rows, type);
        const auto& result = rows.query.Update(view.catalog, query, view.filters.advancedRecordFilters, view.filters.advancedRecordFilterRevision);
        FormTableActions actions{
            .primary = [&](const FormEntry& entry) { Emit(requests, view, entry, primary); },
            .rowContext = [&](const FormEntry& entry, bool multiple) { DrawContext(entry, multiple ? ContextScope::Selection : ContextScope::Single, rows.contextQuantities, view, requests); },
            .canPrimary = [primary](const FormEntry& entry) { return !entry.isDeleted && SupportsRecordAction(entry.category, primary); },
            .selected = [&](auto id) { requests.recentSelections.push_back(id); }
        };
        if (secondary) actions.bulkSecondary = [&](const std::vector<FormEntry>& entries) {
            for (const auto& entry : entries) Emit(requests, view, entry, *secondary);
        };
        auto tableConfig = config;
        tableConfig.copyFormat = view.copyFormat;
        FormTable::DrawPrepared(rows.table, result, tableConfig, actions, &view.favorites);
    }

    void ActivateCategory(BrowserState& state, const BrowserView& view, const char* type)
    {
        if (!state.activeCategory.empty() && state.activeCategory != type && view.autoFocusSearch) state.focusPending = true;
        state.activeCategory = type;
    }
}
