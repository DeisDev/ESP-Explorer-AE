#include "GUI/Tabs/CellBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFilterCache.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <RE/T/TESObjectCELL.h>

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        const std::vector<FormEntry>& GetFilteredEntries(
            RecordFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            const CellBrowserTab::FilterEntriesFn& filterEntries)
        {
            return RecordFilterCache::GetFiltered(cacheState, source, showPlayable, showNonPlayable, showNamed, showUnnamed, showDeleted, filterEntries);
        }
    }

    void CellBrowserTab::Draw(
        const FormCache& cache,
        char* searchBuffer,
        std::size_t searchBufferSize,
        std::string& searchText,
        std::string_view selectedPluginFilter,
        bool& showPlayableRecords,
        bool& showNonPlayableRecords,
        bool& showNamedRecords,
        bool& showUnnamedRecords,
        bool& showDeletedRecords,
        bool* searchFocusPending,
        std::unordered_set<std::uint32_t>& favoriteForms,
        const std::function<void()>& drawPluginFilterStatus,
        const std::function<void()>& persistListFilters,
        const std::function<void()>&,
        const FilterEntriesFn& filterEntries,
        const LocalizeFn& localize,
        const ContextMenuCallbacks* contextCallbacks)
    {
        if (RecordFiltersWidget::Draw(
                localize,
                "CellBrowser",
                RecordFilterState{
                    .showNonPlayable = showNonPlayableRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords })) {
            persistListFilters();
        }

        SearchBar::Draw(localize("Cells", "sSearch", "Cell Search"), searchBuffer, searchBufferSize, searchText, searchFocusPending);
        drawPluginFilterStatus();
        ImGui::Separator();

        const FormTableConfig tableConfig{
            .tableId = "CellTable",
            .primaryActionLabel = localize("Cells", "sTeleport", "Teleport"),
            .quantityActionLabel = nullptr,
            .allowFavorites = true,
            .disableBulkPrimaryAction = true
        };

        static RecordFilterCache cellFilterCache{};
        const auto& filteredCells = GetFilteredEntries(
            cellFilterCache,
            cache.cells,
            showPlayableRecords,
            showNonPlayableRecords,
            showNamedRecords,
            showUnnamedRecords,
            showDeletedRecords,
            filterEntries);

        ImGui::TextDisabled("%zu %s", filteredCells.size(), localize("Cells", "sResults", "cells"));

        const auto requestTeleport = [&](std::uint32_t formID) {
            if (contextCallbacks && contextCallbacks->requestActionConfirmation) {
                contextCallbacks->requestActionConfirmation(
                    localize("General", "sConfirmTeleportTitle", "Confirm Teleport"),
                    localize("General", "sConfirmTeleport", "Teleport to selected destination?"),
                    [formID]() {
                        FormActions::TeleportToCell(formID);
                    });
                return;
            }

            FormActions::TeleportToCell(formID);
        };

        FormTable::Draw(
            filteredCells,
            searchText,
            selectedPluginFilter,
            tableConfig,
            [&](const FormEntry& entry) {
                requestTeleport(entry.formID);
            },
            {},
            {},
            &favoriteForms,
            contextCallbacks);
    }
}
