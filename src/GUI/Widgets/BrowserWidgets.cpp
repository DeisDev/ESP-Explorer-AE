#include "GUI/Widgets/BrowserWidgets.h"

#include "Core/RecordActions.h"
#include "GUI/Widgets/ActionFeedback.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"
#include "GUI/Widgets/SearchControls.h"

#include <imgui.h>

namespace ESPExplorerAE::BrowserWidgets
{
    void DrawControls(BrowserState& state, const BrowserView& view, BrowserRequests& requests,
        const char* id, const char* section, const char* searchFallback)
    {
        auto& filters = view.filters;
        SearchBar::Draw(view.localize(section, "sSearch", searchFallback), state.searchBuffer.data(), state.searchBuffer.size(), state.search, &state.focusPending, id,
            view.localize("General", "sClearSearchButton", "X"));
        DrawSearchControls(state.structuredSearch, state.scope, state.search, state.searchBuffer.data(), state.searchBuffer.size(), *view.catalog, view.localize);
        if (RecordFiltersWidget::Draw(view.localize, id, RecordFilterState{
                .showNonPlayable = filters.showNonPlayableRecords, .showUnnamed = filters.showUnnamedRecords,
                .showDeleted = filters.showDeletedRecords, .advancedRules = filters.advancedRecordFilters,
                .hiddenPlugins = filters.hiddenPlugins }, state.filterEditor, view.catalog)) {
            ++filters.advancedRecordFilterRevision;
            requests.filtersChanged = true;
        }
        const BrowserCategoryState emptyRows;
        auto explanation = MakeQuery(state, view, emptyRows, {});
        if (const auto found = state.categories.find(state.activeCategory); found != state.categories.end()) {
            if (const auto* previous = found->second.query.Criteria()) { explanation.type = previous->type; explanation.types = previous->types; explanation.npc = previous->npc; }
        }
        RecordFiltersWidget::DrawWhyHidden(view.localize, id, {filters.showNonPlayableRecords, filters.showUnnamedRecords, filters.showDeletedRecords,
            filters.advancedRecordFilters, filters.hiddenPlugins}, state.filterEditor, explanation, view.catalog);
        ImGuiWidgetUtils::DrawStatusArea("##BrowserFeedback", [&] {
            ActionFeedback::Draw(state.admission, view.localize);
            DrawSearchFeedback(state.structuredSearch, state.scope, state.search, view.localize);
        });
        ImGui::Separator();
    }

    CatalogQuery MakeQuery(const BrowserState& state, const BrowserView& view, const BrowserCategoryState& rows, std::string type)
    {
        const auto& filters = view.filters;
        return { .type = std::move(type), .search = state.search,
            .showPlayable = filters.showPlayableRecords, .showNonPlayable = filters.showNonPlayableRecords,
            .showNamed = filters.showNamedRecords, .showUnnamed = filters.showUnnamedRecords, .showDeleted = filters.showDeletedRecords,
            .sortColumn = rows.table.sort.column, .ascending = rows.table.sort.ascending, .hiddenPlugins = filters.hiddenPlugins,
            .structuredSearch = state.structuredSearch, .scope = state.scope };
    }

    void DrawCategory(BrowserState& state, const BrowserView& view, BrowserRequests& requests,
        const char* category, const FormTableConfig& config, ActionKind primary, std::optional<ActionKind> secondary,
        std::span<const std::string> types)
    {
        auto& rows = state.categories[category];
        auto query = MakeQuery(state, view, rows, types.empty() ? category : "");
        query.types.assign(types.begin(), types.end());
        const auto& result = rows.query.Update(view.catalog, query, view.filters.advancedRecordFilters, view.filters.advancedRecordFilterRevision);
        FormTableActions actions{
            .primary = [&](const FormEntry& entry) { Emit(requests, view, entry, primary); },
            .rowContext = [&](const FormEntry& entry, bool multiple) { DrawContext(entry, multiple ? ContextScope::Selection : ContextScope::Single, rows.contextQuantities, view, requests); },
            .canPrimary = [primary](const FormEntry& entry) { return !entry.isDeleted && SupportsRecordAction(entry.category, primary); },
            .selected = [&](auto id) { requests.recentSelections.push_back(id); },
            .inspect = [&](auto id) { requests.inspections.push_back(id); },
            .basket = [&](const auto& entries) { for (const auto& entry : entries) requests.basket.push_back(entry.formID); },
            .collect = [&](const auto& entries) { for (const auto& entry : entries) requests.collections.push_back(entry.formID); }
        };
        if (secondary) actions.bulkSecondary = [&](const std::vector<FormEntry>& entries) {
            for (const auto& entry : entries) Emit(requests, view, entry, *secondary);
        };
        auto tableConfig = config;
        tableConfig.copyFormat = view.copyFormat;
        tableConfig.doubleClickGameplayAction = view.doubleClickGameplayAction;
        tableConfig.compactDensity = view.compactTableDensity;
        FormTable::DrawPrepared(rows.table, result, tableConfig, actions, &view.favorites);
    }

    void ActivateCategory(BrowserState& state, const BrowserView& view, const char* type)
    {
        if (!state.activeCategory.empty() && state.activeCategory != type && view.autoFocusSearch) state.focusPending = true;
        state.activeCategory = type;
    }
}
