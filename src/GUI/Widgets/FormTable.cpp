#include "GUI/Widgets/FormTable.h"

#include "GUI/Widgets/FormActions.h"

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
        std::unordered_map<std::string, bool> searchCaseSensitive{};
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

        const char* TryGetEditorID(std::uint32_t formID);
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

        bool ContainsCaseInsensitive(std::string_view text, std::string_view query)
        {
            if (query.empty()) {
                return true;
            }

            std::string textLower(text.begin(), text.end());
            std::string queryLower(query.begin(), query.end());

            std::ranges::transform(textLower, textLower.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            std::ranges::transform(queryLower, queryLower.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });

            return textLower.find(queryLower) != std::string::npos;
        }

        bool ContainsByMode(std::string_view text, std::string_view query, bool caseSensitive)
        {
            if (query.empty()) {
                return true;
            }

            if (caseSensitive) {
                return text.find(query) != std::string::npos;
            }

            return ContainsCaseInsensitive(text, query);
        }

        bool MatchesSearch(const FormEntry& entry, std::string_view query, bool caseSensitive)
        {
            if (query.empty()) {
                return true;
            }

            char formIDBuffer[16]{};
            std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);

            if (ContainsByMode(entry.name, query, caseSensitive) ||
                ContainsByMode(entry.sourcePlugin, query, caseSensitive) ||
                ContainsByMode(entry.category, query, caseSensitive) ||
                ContainsByMode(formIDBuffer, query, caseSensitive)) {
                return true;
            }

            const char* editorID = TryGetEditorID(entry.formID);
            return editorID && ContainsByMode(editorID, query, caseSensitive);
        }

        bool IsPerkCategory(std::string_view category)
        {
            return category == "Perk" || category == "PERK";
        }

        bool IsSpellCategory(std::string_view category)
        {
            return category == "Spell" || category == "SPEL";
        }

        bool IsQuestCategory(std::string_view category)
        {
            return category == "Quest" || category == "QUST";
        }

        bool IsTeleportCategory(std::string_view category)
        {
            return category == "CELL" || category == "WRLD" || category == "LCTN" || category == "REGN";
        }

        bool IsSpawnCategory(std::string_view category)
        {
            return category == "NPC" || category == "NPC_" || category == "LVLN";
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

    void FormTable::Draw(
        const std::vector<FormEntry>& sourceEntries,
        std::string_view searchText,
        std::string_view pluginFilter,
        const FormTableConfig& config,
        const PrimaryAction& primaryAction,
        const QuantityAction& quantityAction,
        std::unordered_set<std::uint32_t>* favorites,
        bool* caseSensitiveOverride)
    {
        const std::string tableId = config.tableId ? config.tableId : "FormTable";

        auto& sortState = sortStates[tableId];
        auto& selected = selectedRows[tableId];
        auto& tableCaseSensitive = searchCaseSensitive[tableId];
        bool caseSensitive = caseSensitiveOverride ? *caseSensitiveOverride : tableCaseSensitive;
        if (caseSensitiveOverride) {
            tableCaseSensitive = *caseSensitiveOverride;
        }

        const auto& entries = GetProcessedEntries(tableId, sourceEntries, searchText, pluginFilter, caseSensitive, sortState);

        ImGui::Spacing();

        auto drawWrappedButton = [](const char* label, bool& firstInRow) {
            const auto& style = ImGui::GetStyle();
            const float buttonWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;

            if (!firstInRow) {
                const float needed = style.ItemSpacing.x + buttonWidth;
                if (ImGui::GetContentRegionAvail().x >= needed) {
                    ImGui::SameLine();
                } else {
                    firstInRow = true;
                }
            }

            const bool pressed = ImGui::Button(label);
            firstInRow = false;
            return pressed;
        };

        bool firstActionInRow = true;

        if (drawWrappedButton(L("General", "sSelectVisible", "Select Visible"), firstActionInRow)) {
            selected.clear();
            for (const auto& entry : entries) {
                selected.insert(entry.formID);
            }
        }

        if (drawWrappedButton(L("General", "sClearSelection", "Clear Selection"), firstActionInRow)) {
            selected.clear();
        }

        const std::string bulkButtonLabel = std::string(config.primaryActionLabel ? config.primaryActionLabel : "Action") + " " + L("General", "sSelected", "Selected");
        const bool hasSelection = !selected.empty();
        if (!hasSelection) {
            ImGui::BeginDisabled(true);
        }
        if (drawWrappedButton(bulkButtonLabel.c_str(), firstActionInRow)) {
            for (const auto& entry : entries) {
                if (selected.contains(entry.formID) && primaryAction) {
                    primaryAction(entry);
                }
            }
        }
        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        if (quantityAction && config.quantityActionLabel) {
            if (!hasSelection) {
                ImGui::BeginDisabled(true);
            }
            if (drawWrappedButton(config.quantityActionLabel, firstActionInRow)) {
                ImGui::OpenPopup("BulkQuantityPopup");
            }
            if (!hasSelection) {
                ImGui::EndDisabled();
            }

            if (ImGui::BeginPopup("BulkQuantityPopup")) {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    ImGui::CloseCurrentPopup();
                }

                ImGui::InputInt(L("General", "sBulkQuantity", "Bulk Quantity"), &pendingBulkQuantity, 1, 10);
                if (pendingBulkQuantity < 1) {
                    pendingBulkQuantity = 1;
                }

                if (ImGui::Button(L("General", "sApplyBulkQuantityAction", "Apply Bulk Quantity Action"))) {
                    for (const auto& entry : entries) {
                        if (selected.contains(entry.formID)) {
                            quantityAction(entry, pendingBulkQuantity);
                        }
                    }
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        if (config.allowFavorites && favorites) {
            if (!hasSelection) {
                ImGui::BeginDisabled(true);
            }
            if (drawWrappedButton(L("General", "sToggleFavoritesSelected", "Toggle Favorites (Selected)"), firstActionInRow)) {
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

                ImGui::TableSetColumnIndex(0);
                std::snprintf(formIDBuffer, sizeof(formIDBuffer), "%08X", entry.formID);
                const std::string rowSelectableLabel = std::string(formIDBuffer) + "##row" + std::to_string(entry.formID) + std::string(config.tableId);
                const bool rowClicked = ImGui::Selectable(rowSelectableLabel.c_str(), rowIsSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, ImGui::GetTextLineHeight()));
                if (rowClicked) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (rowIsSelected) {
                            selected.erase(entry.formID);
                        } else {
                            selected.insert(entry.formID);
                        }
                    } else {
                        selected.clear();
                        selected.insert(entry.formID);
                    }
                }
                ImGui::OpenPopupOnItemClick(rowPopupId.c_str(), ImGuiPopupFlags_MouseButtonRight);

                ImGui::TableSetColumnIndex(1);
                const auto* displayName = entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : entry.name.c_str();
                if (isFavoriteRow) {
                    std::string favoriteName = std::string("★ ") + displayName;
                    ImGui::TextUnformatted(favoriteName.c_str());
                } else {
                    ImGui::TextUnformatted(displayName);
                }
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
                ImGui::OpenPopupOnItemClick(rowPopupId.c_str(), ImGuiPopupFlags_MouseButtonRight);

                if (ImGui::BeginPopup(rowPopupId.c_str())) {
                    if (ImGui::MenuItem(rowIsSelected ? L("General", "sDeselect", "Deselect") : L("General", "sSelect", "Select"))) {
                        if (rowIsSelected) {
                            selected.erase(entry.formID);
                        } else {
                            selected.insert(entry.formID);
                        }
                    }

                    ImGui::Separator();

                    if (primaryAction) {
                        const char* actionLabel = config.primaryActionLabel ? config.primaryActionLabel : "Action";
                        if (ImGui::MenuItem(actionLabel)) {
                            primaryAction(entry);
                        }
                    }

                    if (quantityAction && config.quantityActionLabel) {
                        if (pendingQuantityFormID != entry.formID) {
                            pendingQuantityFormID = entry.formID;
                            pendingQuantity = 1;
                        }

                        if (ImGui::MenuItem(config.quantityActionLabel)) {
                            quantityAction(entry, pendingQuantity);
                        }

                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::InputInt(L("General", "sQuantity", "Quantity"), &pendingQuantity, 1, 10);
                        if (pendingQuantity < 1) {
                            pendingQuantity = 1;
                        }
                    }

                    if (IsSpawnCategory(entry.category) && !quantityAction) {
                        if (pendingQuantityFormID != entry.formID) {
                            pendingQuantityFormID = entry.formID;
                            pendingQuantity = 1;
                        }

                        if (ImGui::MenuItem(L("NPCs", "sSpawnAtPlayer", "Spawn At Player"))) {
                            FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(pendingQuantity));
                        }

                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::InputInt(L("General", "sQuantity", "Quantity"), &pendingQuantity, 1, 10);
                        if (pendingQuantity < 1) {
                            pendingQuantity = 1;
                        }
                    }

                    if (IsSpellCategory(entry.category)) {
                        if (!primaryAction && ImGui::MenuItem(L("General", "sAddSpell", "Add Spell"))) {
                            FormActions::AddSpellToPlayer(entry.formID);
                        }
                        if (ImGui::MenuItem(L("General", "sRemoveSpell", "Remove Spell"))) {
                            FormActions::RemoveSpellFromPlayer(entry.formID);
                        }
                    }

                    if (IsPerkCategory(entry.category)) {
                        if (!primaryAction && ImGui::MenuItem(L("General", "sAddPerk", "Add Perk"))) {
                            FormActions::AddPerkToPlayer(entry.formID);
                        }
                        if (ImGui::MenuItem(L("General", "sRemovePerk", "Remove Perk"))) {
                            FormActions::RemovePerkFromPlayer(entry.formID);
                        }
                    }

                if (IsQuestCategory(entry.category)) {
                    if (ImGui::MenuItem(L("General", "sStartQuest", "Start Quest"))) {
                        char command[64]{};
                        std::snprintf(command, sizeof(command), "startquest %08X", entry.formID);
                        FormActions::ExecuteConsoleCommand(command);
                    }
                    if (ImGui::MenuItem(L("General", "sCompleteQuest", "Complete Quest"))) {
                        char command[64]{};
                        std::snprintf(command, sizeof(command), "completequest %08X", entry.formID);
                        FormActions::ExecuteConsoleCommand(command);
                    }
                }

                if (IsTeleportCategory(entry.category)) {
                    const char* editorID = TryGetEditorID(entry.formID);
                    if (editorID) {
                        if (ImGui::MenuItem(L("General", "sTeleportCOC", "Teleport (COC)"))) {
                            std::string command = std::string("coc ") + editorID;
                            FormActions::ExecuteConsoleCommand(command);
                        }
                    } else {
                        ImGui::BeginDisabled(true);
                        ImGui::MenuItem(L("General", "sTeleportCOC", "Teleport (COC)"));
                        ImGui::EndDisabled();
                    }
                }

                ImGui::Separator();
                if (ImGui::MenuItem(L("General", "sCopyFormID", "Copy FormID"))) {
                    FormActions::CopyFormID(entry.formID);
                }

                if (const char* editorID = TryGetEditorID(entry.formID)) {
                    if (ImGui::MenuItem(L("General", "sCopyEditorID", "Copy EditorID"))) {
                        ImGui::SetClipboardText(editorID);
                    }
                }

                if (ImGui::MenuItem(L("General", "sCopyRecordSource", "Copy Record Source"))) {
                    ImGui::SetClipboardText(entry.sourcePlugin.c_str());
                }

                if (ImGui::MenuItem(L("General", "sCopyName", "Copy Name"))) {
                    ImGui::SetClipboardText(displayName);
                }

                if (config.allowFavorites && favorites) {
                    ImGui::Separator();
                    const bool isFavorite = favorites->contains(entry.formID);
                    if (ImGui::MenuItem(isFavorite ? L("General", "sRemoveFavorite", "Remove Favorite") : L("General", "sAddFavorite", "Add Favorite"))) {
                        if (isFavorite) {
                            favorites->erase(entry.formID);
                        } else {
                            favorites->insert(entry.formID);
                        }
                    }
                }

                    ImGui::EndPopup();
                }
            }
        }

        ImGui::EndTable();

        ImGui::TextDisabled("%s: %zu  |  %s: %zu", L("General", "sVisible", "Visible"), entries.size(), L("General", "sSelectedShort", "Sel"), selected.size());

        const FormEntry* selectedEntry = nullptr;
        for (const auto& entry : entries) {
            if (selected.contains(entry.formID)) {
                selectedEntry = &entry;
                break;
            }
        }

        if (selectedEntry) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool firstSelectedActionInRow = true;
            if (primaryAction && drawWrappedButton(config.primaryActionLabel ? config.primaryActionLabel : "Action", firstSelectedActionInRow)) {
                primaryAction(*selectedEntry);
            }

            if (quantityAction && config.quantityActionLabel) {
                if (drawWrappedButton(config.quantityActionLabel, firstSelectedActionInRow)) {
                    quantityAction(*selectedEntry, pendingQuantity);
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

            if (drawWrappedButton(L("General", "sCopyFormID", "Copy FormID"), firstSelectedActionInRow)) {
                FormActions::CopyFormID(selectedEntry->formID);
            }
        }
    }
}
