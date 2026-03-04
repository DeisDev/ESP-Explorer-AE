#include "GUI/Tabs/CellBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <RE/T/TESObjectCELL.h>

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        struct LocalFilterCache
        {
            const std::vector<FormEntry>* source{ nullptr };
            std::size_t sourceSize{ 0 };
            bool showPlayable{ true };
            bool showNonPlayable{ true };
            bool showNamed{ true };
            bool showUnnamed{ true };
            bool showDeleted{ true };
            std::vector<FormEntry> filtered{};
        };

        const std::vector<FormEntry>& GetFilteredEntries(
            LocalFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            const CellBrowserTab::FilterEntriesFn& filterEntries)
        {
            const bool needsRebuild =
                cacheState.source != &source ||
                cacheState.sourceSize != source.size() ||
                cacheState.showPlayable != showPlayable ||
                cacheState.showNonPlayable != showNonPlayable ||
                cacheState.showNamed != showNamed ||
                cacheState.showUnnamed != showUnnamed ||
                cacheState.showDeleted != showDeleted;

            if (needsRebuild) {
                cacheState.filtered = filterEntries(source);
                cacheState.source = &source;
                cacheState.sourceSize = source.size();
                cacheState.showPlayable = showPlayable;
                cacheState.showNonPlayable = showNonPlayable;
                cacheState.showNamed = showNamed;
                cacheState.showUnnamed = showUnnamed;
                cacheState.showDeleted = showDeleted;
            }

            return cacheState.filtered;
        }

        const char* TryGetEditorID(std::uint32_t formID)
        {
            auto* form = RE::TESForm::GetFormByID(formID);
            if (!form) {
                return nullptr;
            }

            const auto* editorID = form->GetFormEditorID();
            if (!editorID || editorID[0] == '\0') {
                return nullptr;
            }

            return editorID;
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

        static LocalFilterCache cellFilterCache{};
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

        FormTable::Draw(
            filteredCells,
            searchText,
            selectedPluginFilter,
            tableConfig,
            [](const FormEntry& entry) {
                FormActions::TeleportToCell(entry.formID);
            },
            {},
            {},
            &favoriteForms,
            contextCallbacks);
    }
}
