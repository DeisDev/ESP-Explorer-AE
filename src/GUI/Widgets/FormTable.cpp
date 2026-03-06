#include "GUI/Widgets/FormTable.h"

#include "GUI/Widgets/ContextMenu.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/SharedUtils.h"

#include "Localization/Language.h"

#include <imgui.h>

#include <cctype>
#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        struct SortState
        {
            int column{ 1 };
            bool ascending{ true };
        };

        std::unordered_map<std::string, SortState> sortStates{};
        std::unordered_map<std::string, std::unordered_set<std::uint32_t>> selectedRows{};
        std::unordered_map<std::string, int> lastClickedIndex{};
        struct TableRenderCache
        {
            const std::vector<FormEntry>* source{ nullptr };
            std::size_t sourceSize{ 0 };
            std::string search{};
            std::string pluginFilter{};
            bool caseSensitive{ false };
            int sortColumn{ 1 };
            bool sortAscending{ true };
            std::vector<FormEntry> entries{};
        };
        std::unordered_map<std::string, TableRenderCache> tableRenderCaches{};
        std::uint32_t pendingQuantityFormID{ 0 };
        int pendingQuantity{ 1 };
        int pendingBulkQuantity{ 1 };

        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        }

        bool MatchesSearch(const FormEntry& entry, std::string_view query, bool caseSensitive);

        const std::vector<FormEntry>& GetProcessedEntries(
            std::string_view tableId,
            const std::vector<FormEntry>& sourceEntries,
            std::string_view searchText,
            std::string_view pluginFilter,
            bool caseSensitive,
            const SortState& sortState)
        {
            auto& cache = tableRenderCaches[std::string(tableId)];

            const bool needsRebuild =
                cache.source != &sourceEntries ||
                cache.sourceSize != sourceEntries.size() ||
                cache.search != searchText ||
                cache.pluginFilter != pluginFilter ||
                cache.caseSensitive != caseSensitive ||
                cache.sortColumn != sortState.column ||
                cache.sortAscending != sortState.ascending;

            if (!needsRebuild) {
                return cache.entries;
            }

            cache.entries = sourceEntries;

            if (!pluginFilter.empty()) {
                cache.entries.erase(std::remove_if(cache.entries.begin(), cache.entries.end(), [&](const FormEntry& entry) {
                    return entry.sourcePlugin != pluginFilter;
                }), cache.entries.end());
            }

            if (!searchText.empty()) {
                cache.entries.erase(std::remove_if(cache.entries.begin(), cache.entries.end(), [&](const FormEntry& entry) {
                    return !MatchesSearch(entry, searchText, caseSensitive);
                }), cache.entries.end());
            }

            std::ranges::sort(cache.entries, [&](const FormEntry& left, const FormEntry& right) {
                const auto compareBy = [&](int column) {
                    switch (column) {
                    case 0:
                        if (left.formID < right.formID) {
                            return -1;
                        }
                        if (left.formID > right.formID) {
                            return 1;
                        }
                        return 0;
                    case 1:
                        return left.name.compare(right.name);
                    case 2:
                        return left.sourcePlugin.compare(right.sourcePlugin);
                    default:
                        return left.name.compare(right.name);
                    }
                };

                const int cmp = compareBy(sortState.column);
                if (cmp == 0) {
                    return left.formID < right.formID;
                }
                return sortState.ascending ? (cmp < 0) : (cmp > 0);
            });

            cache.source = &sourceEntries;
            cache.sourceSize = sourceEntries.size();
            cache.search = searchText;
            cache.pluginFilter = pluginFilter;
            cache.caseSensitive = caseSensitive;
            cache.sortColumn = sortState.column;
            cache.sortAscending = sortState.ascending;

            return cache.entries;
        }

        bool MatchesSearch(const FormEntry& entry, std::string_view query, bool caseSensitive)
        {
            if (query.empty()) {
                return true;
            }

            char formIDBuffer[16]{};
            std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);

            if (SharedUtils::ContainsByMode(entry.name, query, caseSensitive) ||
                SharedUtils::ContainsByMode(entry.sourcePlugin, query, caseSensitive) ||
                SharedUtils::ContainsByMode(entry.category, query, caseSensitive) ||
                SharedUtils::ContainsByMode(formIDBuffer, query, caseSensitive)) {
                return true;
            }

            const char* editorID = ContextMenu::TryGetEditorID(entry.formID);
            return editorID && SharedUtils::ContainsByMode(editorID, query, caseSensitive);
        }
    }

    void FormTable::Draw(
        const std::vector<FormEntry>& sourceEntries,
        std::string_view searchText,
        std::string_view pluginFilter,
        const FormTableConfig& config,
        const PrimaryAction& primaryAction,
        const BulkPrimaryAction& bulkPrimaryAction,
        const QuantityAction& quantityAction,
        std::unordered_set<std::uint32_t>* favorites,
        const ContextMenuCallbacks* contextCallbacks,
        const BulkSecondaryAction& bulkSecondaryAction)
    {
        const std::string tableId = config.tableId ? config.tableId : "FormTable";

        auto& sortState = sortStates[tableId];
        auto& selected = selectedRows[tableId];
        auto& lastClicked = lastClickedIndex[tableId];

        const auto& entries = GetProcessedEntries(tableId, sourceEntries, searchText, pluginFilter, false, sortState);

        auto invokePrimaryForSelection = [&]() {
            if (!primaryAction && !bulkPrimaryAction) {
                return;
            }

            std::vector<FormEntry> selectedEntries{};
            selectedEntries.reserve(selected.size());
            for (const auto& entry : entries) {
                if (selected.contains(entry.formID)) {
                    selectedEntries.push_back(entry);
                }
            }

            if (selectedEntries.empty()) {
                return;
            }

            if (bulkPrimaryAction) {
                bulkPrimaryAction(selectedEntries);
                return;
            }

            for (const auto& entry : selectedEntries) {
                primaryAction(entry);
            }
        };

        auto collectSelectedEntries = [&]() {
            std::vector<FormEntry> selectedEntries{};
            selectedEntries.reserve(selected.size());
            for (const auto& e : entries) {
                if (selected.contains(e.formID)) {
                    selectedEntries.push_back(e);
                }
            }
            return selectedEntries;
        };

        ImGui::Spacing();

        auto drawWrappedButton = SharedUtils::DrawWrappedButton;

        bool firstActionInRow = true;

        if (drawWrappedButton(L("General", "sClearSelection", "Clear Selection"), firstActionInRow)) {
            selected.clear();
        }

        const std::string bulkButtonLabel = std::string(config.primaryActionLabel ? config.primaryActionLabel : "Action") + " " + L("General", "sSelected", "Selected");
        const bool hasSelection = !selected.empty();
        const bool bulkPrimaryDisabled = !hasSelection || (config.disableBulkPrimaryAction && selected.size() > 1);
        if (bulkPrimaryDisabled) {
            ImGui::BeginDisabled(true);
        }
        if (primaryAction || bulkPrimaryAction) {
            if (drawWrappedButton(bulkButtonLabel.c_str(), firstActionInRow)) {
                invokePrimaryForSelection();
            }
        }
        if (quantityAction) {
            const char* qtyLabel = config.quantityActionLabel ? config.quantityActionLabel : L("NPCs", "sSpawnAtPlayer", "Spawn At Player");
            const std::string bulkQtyLabel = std::string(qtyLabel) + " " + L("General", "sSelected", "Selected");
            if (drawWrappedButton(bulkQtyLabel.c_str(), firstActionInRow)) {
                for (const auto& entry : entries) {
                    if (selected.contains(entry.formID)) {
                        quantityAction(entry, pendingBulkQuantity);
                    }
                }
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100.0f);
            ImGui::InputInt("##BulkQty", &pendingBulkQuantity, 1, 10);
            if (pendingBulkQuantity < 1) {
                pendingBulkQuantity = 1;
            }
            firstActionInRow = false;
        }
        if (bulkPrimaryDisabled) {
            ImGui::EndDisabled();
        }

        if (config.allowFavorites && favorites) {
            if (!hasSelection) {
                ImGui::BeginDisabled(true);
            }
            const std::string topToggleFavoritesLabel = std::string(L("General", "sToggleFavoritesSelected", "Toggle Favorites (Selected)")) + "##Top" + tableId;
            if (drawWrappedButton(topToggleFavoritesLabel.c_str(), firstActionInRow)) {
                for (const auto& entry : entries) {
                    if (!selected.contains(entry.formID)) {
                        continue;
                    }

                    if (favorites->contains(entry.formID)) {
                        favorites->erase(entry.formID);
                    } else {
                        favorites->insert(entry.formID);
                    }
                }
            }
            if (!hasSelection) {
                ImGui::EndDisabled();
            }
        }

        ImGui::Separator();
        ImGui::Spacing();

        const auto availableHeight = ImGui::GetContentRegionAvail().y - 8.0f;
        const auto tableHeight = availableHeight > 220.0f ? availableHeight : 220.0f;
        if (!ImGui::BeginTable(config.tableId, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
            return;
        }

        ImGui::TableSetupColumn(L("General", "sFormID", "FormID"));
        ImGui::TableSetupColumn(L("General", "sName", "Name"), ImGuiTableColumnFlags_DefaultSort);
        ImGui::TableSetupColumn(L("General", "sPlugin", "Plugin"));
        ImGui::TableHeadersRow();

        if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs(); sortSpecs && sortSpecs->SpecsCount > 0 && sortSpecs->SpecsDirty) {
            sortState.column = sortSpecs->Specs[0].ColumnIndex;
            sortState.ascending = (sortSpecs->Specs[0].SortDirection == ImGuiSortDirection_Ascending);
            sortSpecs->SpecsDirty = false;
        }

        char formIDBuffer[16]{};

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step()) {
            for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                const auto& entry = entries[static_cast<std::size_t>(rowIndex)];
                ImGui::TableNextRow();

                const std::string rowPopupId = "RowContext##" + std::to_string(entry.formID) + std::string(config.tableId);
                const bool rowIsSelected = selected.contains(entry.formID);
                const bool isFavoriteRow = config.allowFavorites && favorites && favorites->contains(entry.formID);
                const auto selectRowOnRightClick = [&]() {
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        if (!selected.contains(entry.formID)) {
                            selected.clear();
                            selected.insert(entry.formID);
                            lastClicked = rowIndex;
                        }
                    }
                };

                ImGui::TableSetColumnIndex(0);
                std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);
                const std::string rowSelectableLabel = std::string(formIDBuffer) + "##row" + std::to_string(entry.formID) + std::string(config.tableId);
                const bool rowClicked = ImGui::Selectable(rowSelectableLabel.c_str(), rowIsSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, ImGui::GetTextLineHeight()));
                if (rowClicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && primaryAction) {
                    primaryAction(entry);
                } else if (rowClicked) {
                    if (ImGui::GetIO().KeyShift && lastClicked >= 0 && lastClicked < static_cast<int>(entries.size())) {
                        const int rangeStart = (std::min)(lastClicked, rowIndex);
                        const int rangeEnd = (std::max)(lastClicked, rowIndex);
                        if (!ImGui::GetIO().KeyCtrl) {
                            selected.clear();
                        }
                        for (int i = rangeStart; i <= rangeEnd; ++i) {
                            selected.insert(entries[static_cast<std::size_t>(i)].formID);
                        }
                    } else if (ImGui::GetIO().KeyCtrl) {
                        if (rowIsSelected) {
                            selected.erase(entry.formID);
                        } else {
                            selected.insert(entry.formID);
                        }
                    } else {
                        selected.clear();
                        selected.insert(entry.formID);
                    }
                    lastClicked = rowIndex;
                }

                const bool openContextFromNav = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false);
                if (openContextFromNav) {
                    if (!selected.contains(entry.formID)) {
                        selected.clear();
                        selected.insert(entry.formID);
                        lastClicked = rowIndex;
                    }
                    ImGui::OpenPopup(rowPopupId.c_str());
                }

                selectRowOnRightClick();
                ImGui::OpenPopupOnItemClick(rowPopupId.c_str(), ImGuiPopupFlags_MouseButtonRight);

                ImGui::TableSetColumnIndex(1);
                const auto* displayName = entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : entry.name.c_str();
                if (isFavoriteRow) {
                    std::string favoriteName = std::string("★ ") + displayName;
                    ImGui::TextUnformatted(favoriteName.c_str());
                } else {
                    ImGui::TextUnformatted(displayName);
                }
                selectRowOnRightClick();
                ImGui::OpenPopupOnItemClick(rowPopupId.c_str(), ImGuiPopupFlags_MouseButtonRight);

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s: %s", L("General", "sName", "Name"), displayName);
                    ImGui::Text("%s: %s", L("General", "sCategory", "Category"), entry.category.c_str());
                    ImGui::Text("%s: %s", L("General", "sPlugin", "Plugin"), entry.sourcePlugin.c_str());
                    ImGui::Text("%s: %s", L("General", "sDeleted", "Deleted"), entry.isDeleted ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));
                    ImGui::Text("%s: %s", L("General", "sPlayable", "Playable"), entry.isPlayable ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));
                    if (config.allowFavorites && favorites) {
                        ImGui::Text("%s: %s", L("General", "sFavorite", "Favorite"), isFavoriteRow ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));
                    }
                    ImGui::EndTooltip();
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(entry.sourcePlugin.c_str());
                selectRowOnRightClick();
                ImGui::OpenPopupOnItemClick(rowPopupId.c_str(), ImGuiPopupFlags_MouseButtonRight);

                if (ImGui::BeginPopup(rowPopupId.c_str())) {
                    const bool popupRowIsSelected = selected.contains(entry.formID);
                    const bool multipleSelected = selected.size() > 1 && popupRowIsSelected;

                    if (multipleSelected) {
                        ImGui::TextDisabled("%zu %s", selected.size(), L("General", "sItemsSelected", "items selected"));
                        ImGui::Separator();
                    }

                    if (ImGui::MenuItem(popupRowIsSelected ? L("General", "sDeselect", "Deselect") : L("General", "sSelect", "Select"))) {
                        if (popupRowIsSelected) {
                            selected.erase(entry.formID);
                        } else {
                            selected.insert(entry.formID);
                        }
                    }

                    ImGui::Separator();

                    if (multipleSelected) {
                        const auto selectedEntries = collectSelectedEntries();

                        if ((primaryAction || bulkPrimaryAction) && !config.disableBulkPrimaryAction) {
                            const char* actionLabel = config.primaryActionLabel ? config.primaryActionLabel : "Action";
                            std::string bulkLabel = std::string(actionLabel) + " (" + std::to_string(selected.size()) + ")";
                            if (ImGui::MenuItem(bulkLabel.c_str())) {
                                invokePrimaryForSelection();
                            }
                        }

                        if (bulkSecondaryAction && config.secondaryActionLabel) {
                            std::string bulkSecondaryLabel = std::string(config.secondaryActionLabel) + " (" + std::to_string(selected.size()) + ")";
                            if (ImGui::MenuItem(bulkSecondaryLabel.c_str())) {
                                bulkSecondaryAction(selectedEntries);
                            }
                        }

                        if (quantityAction) {
                            const char* qtyLabel = config.quantityActionLabel ? config.quantityActionLabel : L("NPCs", "sSpawnAtPlayer", "Spawn At Player");
                            std::string bulkQtyLabel = std::string(qtyLabel) + " (" + std::to_string(selected.size()) + ")";
                            if (ImGui::MenuItem(bulkQtyLabel.c_str())) {
                                for (const auto& e : entries) {
                                    if (selected.contains(e.formID)) {
                                        quantityAction(e, pendingQuantity);
                                    }
                                }
                            }
                        }

                        if (!selectedEntries.empty()) {
                            if (ImGui::MenuItem((std::string(L("General", "sCopyFormID", "Copy FormID")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                                std::vector<std::string> values{};
                                values.reserve(selectedEntries.size());
                                for (const auto& selectedEntry : selectedEntries) {
                                    char idBuffer[16]{};
                                    std::snprintf(idBuffer, sizeof(idBuffer), "%08X", selectedEntry.formID);
                                    values.emplace_back(idBuffer);
                                }
                                const std::string clipboard = SharedUtils::BuildParenthesizedList(values);
                                ImGui::SetClipboardText(clipboard.c_str());
                            }

                            if (ImGui::MenuItem((std::string(L("General", "sCopyRecordSource", "Copy Record Source")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                                std::vector<std::string> values{};
                                values.reserve(selectedEntries.size());
                                for (const auto& selectedEntry : selectedEntries) {
                                    values.push_back(selectedEntry.sourcePlugin);
                                }
                                const std::string clipboard = SharedUtils::BuildParenthesizedList(values);
                                ImGui::SetClipboardText(clipboard.c_str());
                            }

                            if (ImGui::MenuItem((std::string(L("General", "sCopyName", "Copy Name")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                                std::vector<std::string> values{};
                                values.reserve(selectedEntries.size());
                                for (const auto& selectedEntry : selectedEntries) {
                                    values.push_back(selectedEntry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                                }
                                const std::string clipboard = SharedUtils::BuildParenthesizedList(values);
                                ImGui::SetClipboardText(clipboard.c_str());
                            }

                            if (favorites) {
                                if (ImGui::MenuItem((std::string(L("General", "sAddFavorite", "Add Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                                    for (const auto& selectedEntry : selectedEntries) {
                                        favorites->insert(selectedEntry.formID);
                                    }
                                }

                                if (ImGui::MenuItem((std::string(L("General", "sRemoveFavorite", "Remove Favorite")) + " (" + std::to_string(selectedEntries.size()) + ")").c_str())) {
                                    for (const auto& selectedEntry : selectedEntries) {
                                        favorites->erase(selectedEntry.formID);
                                    }
                                }
                            }
                        }

                        ImGui::Separator();
                    }

                    if (contextCallbacks) {
                        ContextMenuCallbacks callbacks = *contextCallbacks;
                        callbacks.hideCopyAndFavoriteActions = multipleSelected;
                        ContextMenu::Draw(entry, callbacks);
                    } else {
                        ContextMenuCallbacks fallbackCallbacks{};
                        fallbackCallbacks.favorites = favorites;
                        fallbackCallbacks.hideCopyAndFavoriteActions = multipleSelected;
                        ContextMenu::Draw(entry, fallbackCallbacks);
                    }

                    ImGui::EndPopup();
                }
            }
        }

        ImGui::EndTable();

        ImGui::TextDisabled("%s: %zu  |  %s: %zu", L("General", "sVisible", "Visible"), entries.size(), L("General", "sSelectedShort", "Sel"), selected.size());

        const FormEntry* selectedEntry = nullptr;
        if (lastClicked >= 0 && lastClicked < static_cast<int>(entries.size())) {
            const auto& lastClickedEntry = entries[static_cast<std::size_t>(lastClicked)];
            if (selected.contains(lastClickedEntry.formID)) {
                selectedEntry = &lastClickedEntry;
            }
        }

        if (!selectedEntry) {
            for (const auto& entry : entries) {
                if (selected.contains(entry.formID)) {
                    selectedEntry = &entry;
                    break;
                }
            }
        }

        if (selectedEntry) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const bool multipleSelected = selected.size() > 1;
            if (multipleSelected) {
                ImGui::TextDisabled("%zu %s", selected.size(), L("General", "sItemsSelected", "items selected"));
            }

            bool firstSelectedActionInRow = true;
            if (primaryAction || bulkPrimaryAction) {
                const bool disableThisAction = multipleSelected && config.disableBulkPrimaryAction;
                const char* label = config.primaryActionLabel ? config.primaryActionLabel : "Action";
                std::string actionLabel = multipleSelected
                    ? std::string(label) + " (" + std::to_string(selected.size()) + ")"
                    : std::string(label);

                if (disableThisAction) {
                    ImGui::BeginDisabled(true);
                }
                if (drawWrappedButton(actionLabel.c_str(), firstSelectedActionInRow)) {
                    if (multipleSelected) {
                        invokePrimaryForSelection();
                    } else if (primaryAction) {
                        primaryAction(*selectedEntry);
                    } else {
                        invokePrimaryForSelection();
                    }
                }
                if (disableThisAction) {
                    ImGui::EndDisabled();
                }
            }

            if (quantityAction) {
                const char* qtyLabel = config.quantityActionLabel ? config.quantityActionLabel : L("NPCs", "sSpawnAtPlayer", "Spawn At Player");
                std::string qtyActionLabel = multipleSelected
                    ? std::string(qtyLabel) + " (" + std::to_string(selected.size()) + ")"
                    : std::string(qtyLabel);

                if (drawWrappedButton(qtyActionLabel.c_str(), firstSelectedActionInRow)) {
                    if (multipleSelected) {
                        for (const auto& e : entries) {
                            if (selected.contains(e.formID)) {
                                quantityAction(e, pendingQuantity);
                            }
                        }
                    } else {
                        quantityAction(*selectedEntry, pendingQuantity);
                    }
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputInt("##SelectedQty", &pendingQuantity, 1, 10);
                if (pendingQuantity < 1) {
                    pendingQuantity = 1;
                }
                firstSelectedActionInRow = false;
            }

            if (config.allowFavorites && favorites) {
                if (multipleSelected) {
                    const std::string bottomToggleFavoritesLabel = std::string(L("General", "sToggleFavoritesSelected", "Toggle Favorites (Selected)")) + "##Bottom" + tableId;
                    if (drawWrappedButton(bottomToggleFavoritesLabel.c_str(), firstSelectedActionInRow)) {
                        for (const auto& e : entries) {
                            if (!selected.contains(e.formID)) continue;
                            if (favorites->contains(e.formID)) {
                                favorites->erase(e.formID);
                            } else {
                                favorites->insert(e.formID);
                            }
                        }
                    }
                } else {
                    const bool isFavorite = favorites->contains(selectedEntry->formID);
                    const char* favoriteLabel = isFavorite ? L("General", "sRemoveFavorite", "Remove Favorite") : L("General", "sAddFavorite", "Add Favorite");
                    if (drawWrappedButton(favoriteLabel, firstSelectedActionInRow)) {
                        if (isFavorite) {
                            favorites->erase(selectedEntry->formID);
                        } else {
                            favorites->insert(selectedEntry->formID);
                        }
                    }
                }
            }

            if (!multipleSelected) {
                if (drawWrappedButton(L("General", "sCopyFormID", "Copy FormID"), firstSelectedActionInRow)) {
                    FormActions::CopyFormID(selectedEntry->formID);
                }
            }
        }
    }
}
