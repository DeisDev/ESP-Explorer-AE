#include "GUI/Tabs/CellBrowserTab.h"
#include "Core/RecordActions.h"
#include "GUI/Widgets/BrowserWidgets.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void CellBrowserTab::Draw(BrowserState& state, const BrowserView& view, BrowserRequests& requests)
    {
        BrowserWidgets::DrawControls(state, view, requests, "CellBrowser", "Cells", "Cell Search");
        auto& rows = state.categories["CELL"];
        const auto query = BrowserWidgets::MakeQuery(state, view, rows, "CELL");
        const auto& result = rows.query.Update(view.catalog, query, view.filters.advancedRecordFilters, view.filters.advancedRecordFilterRevision);
        ImGui::TextDisabled("%zu %s", result.order.size(), view.localize("Cells", "sResults", "cells"));
        const FormTableConfig config{
            .tableId = "CellTable", .primaryActionLabel = view.localize("Cells", "sTeleport", "Teleport"),
            .allowFavorites = true, .disableBulkPrimaryAction = true, .gameplayActionsAllowed = view.gameplayReady, .copyFormat = view.copyFormat
        };
        const FormTableActions actions{
            .primary = [&](const FormEntry& entry) { BrowserWidgets::Emit(requests, view, entry, ActionKind::Teleport, true); },
            .rowContext = [&](const FormEntry& entry, bool multiple) { BrowserWidgets::DrawContext(entry, multiple ? BrowserWidgets::ContextScope::Selection : BrowserWidgets::ContextScope::Single, rows.contextQuantities, view, requests); },
            .canPrimary = CanTeleportRecord,
            .selected = [&](auto id) { requests.recentSelections.push_back(id); }
        };
        FormTable::DrawPrepared(rows.table, result, config, actions, &view.favorites);
    }
}
