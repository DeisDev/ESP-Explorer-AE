#include "GUI/Widgets/FormTable.h"

#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/SharedUtils.h"

#include "Localization/Language.h"

#include <imgui.h>

#include <cctype>
#include <ranges>

namespace ESPExplorerAE
{
    namespace
    {
        const char* L(std::string_view section, std::string_view key, const char* fallback)
        {
            const auto value = Language::FrameText(section, key);
            return value.empty() ? fallback : value.data();
        }


        std::string CopyLabel(const char* label, std::size_t count, const char* id)
        {
            return std::string(label) + " (" + std::to_string(count) + ")###" + id;
        }

    }

    void FormTable::DrawPrepared(FormTableState& state, const CatalogResult& result, const FormTableConfig& config,
        const FormTableActions& actions, std::unordered_set<std::uint32_t>* favorites)
    {
        auto entries = result.order | std::views::transform([&](RecordIndex index) -> const FormEntry& { return result.snapshot->records[index]; });
        const ResultRevision revision{ result.snapshot ? result.snapshot->generation : 0, result.revision };
        const std::string tableId = config.tableId ? config.tableId : "FormTable";
        auto& sortState = state.sort;
        auto& selected = state.selection.selected;
        auto& pendingQuantity = state.quantity;
        auto& pendingBulkQuantity = state.bulkQuantity;
        const auto& primaryAction = actions.primary;
        const auto& bulkPrimaryAction = actions.bulkPrimary;
        const auto& quantityAction = actions.quantity;
        const auto& bulkSecondaryAction = actions.bulkSecondary;
        if (state.orderRevision != revision) {
            state.displayedIDs.clear();
            state.displayedPositions.clear();
            state.displayedIDs.reserve(entries.size());
            for (const auto& entry : entries) {
                state.displayedPositions.emplace(entry.formID, state.displayedIDs.size());
                state.displayedIDs.push_back(entry.formID);
            }
            state.selection.Reconcile(state.displayedIDs);
            state.orderRevision = revision;
        }
        ImGui::PushID(tableId.c_str());

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

        auto trackRecentRecord = [&](const FormEntry& entry) {
            if (actions.selected) actions.selected(entry.formID);
        };

        ImGui::Spacing();

        auto drawWrappedButton = ImGuiWidgetUtils::DrawWrappedButton;
        const bool gameplayActionsAllowed = config.gameplayActionsAllowed;
        const char* disabledTooltip = L("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open.");

        bool firstActionInRow = true;

        if (drawWrappedButton(L("General", "sSelectVisible", "Select Visible"), firstActionInRow)) {
            state.selection.All(state.displayedIDs);
        }

        if (drawWrappedButton(L("General", "sClearSelection", "Clear Selection"), firstActionInRow)) {
            state.selection.Clear();
        }

        const bool hasSelection = !selected.empty();
        const auto primaryAllowed = [&] {
            return !actions.canPrimary || std::ranges::all_of(selected, [&](auto id) {
                return actions.canPrimary(entries[state.displayedPositions.at(id)]);
            });
        };

        if (!hasSelection) {
            ImGui::BeginDisabled(true);
        }
        const auto copyFormat = config.copyFormat;
        const std::string copyFormIDsLabel = CopyLabel(L("General", "sCopyFormID", "Copy FormID"), selected.size(), "CopyIDs");
        const std::string copyNamesLabel = CopyLabel(L("General", "sCopyName", "Copy Name"), selected.size(), "CopyNames");
        if (drawWrappedButton(copyFormIDsLabel.c_str(), firstActionInRow)) {
            const auto selectedEntries = collectSelectedEntries();
            std::vector<std::string> values{};
            values.reserve(selectedEntries.size());
            for (const auto& selectedEntry : selectedEntries) {
                values.push_back(FormatUtils::FormID(selectedEntry.formID));
            }
            const std::string clipboard = FormatUtils::MultiCopyList(values, copyFormat);
            ImGui::SetClipboardText(clipboard.c_str());
        }
        if (drawWrappedButton(copyNamesLabel.c_str(), firstActionInRow)) {
            const auto selectedEntries = collectSelectedEntries();
            std::vector<std::string> values{};
            values.reserve(selectedEntries.size());
            for (const auto& selectedEntry : selectedEntries) {
                values.push_back(selectedEntry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
            }
            const std::string clipboard = FormatUtils::MultiCopyList(values, copyFormat);
            ImGui::SetClipboardText(clipboard.c_str());
        }
        if (!hasSelection) {
            ImGui::EndDisabled();
        }

        const std::string bulkButtonLabel = std::string(config.primaryActionLabel ? config.primaryActionLabel : L("General", "sAction", "Action")) + " " + L("General", "sSelected", "Selected") + "###BulkAction";
        const bool bulkPrimaryDisabled = !gameplayActionsAllowed || !primaryAllowed() || !hasSelection || (config.disableBulkPrimaryAction && selected.size() > 1);
        if (bulkPrimaryDisabled) {
            ImGui::BeginDisabled(true);
        }
        if (primaryAction || bulkPrimaryAction) {
            if (drawWrappedButton(bulkButtonLabel.c_str(), firstActionInRow)) {
                invokePrimaryForSelection();
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
        }
        if (quantityAction) {
            const char* qtyLabel = config.quantityActionLabel ? config.quantityActionLabel : L("NPCs", "sSpawnAtPlayer", "Spawn At Player");
            const std::string bulkQtyLabel = std::string(qtyLabel) + " " + L("General", "sSelected", "Selected") + "###BulkQuantityAction";
            if (drawWrappedButton(bulkQtyLabel.c_str(), firstActionInRow)) {
                for (const auto& entry : entries) {
                    if (selected.contains(entry.formID)) {
                        quantityAction(entry, pendingBulkQuantity);
                    }
                }
            }
            ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
            ImGuiWidgetUtils::SameLineIfFits(140.0f);
            ImVec4 bulkFrameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
            ImVec4 bulkFrameBgHovered = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
            ImVec4 bulkFrameBgActive = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);
            bulkFrameBg.w = (std::max)(bulkFrameBg.w, 0.30f);
            bulkFrameBgHovered.w = (std::max)(bulkFrameBgHovered.w, 0.36f);
            bulkFrameBgActive.w = (std::max)(bulkFrameBgActive.w, 0.42f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, bulkFrameBg);
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bulkFrameBgHovered);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bulkFrameBgActive);
            ImGui::SetNextItemWidth(140.0f);
            ImGui::InputInt("##BulkQty", &pendingBulkQuantity, 1, 10);
            ImGui::PopStyleColor(3);
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

        const auto availableHeight = ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing() - ImGui::GetStyle().ItemSpacing.y;
        const auto tableHeight = (std::max)(ImGui::GetFrameHeight() * 2.0f, availableHeight);
        const ImVec4 headerBase = ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg);
        const ImVec4 borderBase = ImGui::GetStyleColorVec4(ImGuiCol_Border);
        const ImVec4 accentBase = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImVec4(headerBase.x, headerBase.y, headerBase.z, 0.90f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(borderBase.x, borderBase.y, borderBase.z, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accentBase.x, accentBase.y, accentBase.z, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accentBase.x, accentBase.y, accentBase.z, 0.24f));
        if (!ImGui::BeginTable(config.tableId, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY, ImVec2(0.0f, tableHeight))) {
            ImGui::PopStyleColor(4);
            ImGui::PopID();
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

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(entries.size()));
        while (clipper.Step()) {
            for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                const auto& entry = entries[static_cast<std::size_t>(rowIndex)];
                ImGui::PushID(static_cast<int>(entry.formID));
                ImGui::TableNextRow();

                const bool rowIsSelected = selected.contains(entry.formID);
                const bool isFavoriteRow = config.allowFavorites && favorites && favorites->contains(entry.formID);
                const auto selectRowOnRightClick = [&]() {
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                        if (!selected.contains(entry.formID)) {
                            state.selection.Single(entry.formID);
                            trackRecentRecord(entry);
                        }
                    }
                };

                ImGui::TableSetColumnIndex(0);
                const std::string formIDText = FormatUtils::FormID(entry.formID);
                const bool rowClicked = ImGui::Selectable(formIDText.c_str(), rowIsSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, ImGui::GetTextLineHeight()));
                if (rowClicked) {
                    state.selection.Click(state.displayedIDs, entry.formID, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);

                    if (selected.contains(entry.formID)) {
                        trackRecentRecord(entry);
                    }

                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && primaryAction && gameplayActionsAllowed && (!actions.canPrimary || actions.canPrimary(entry))) {
                        primaryAction(entry);
                    }
                }

                const bool rowIsSelectedAfterInteraction = selected.contains(entry.formID);
                SharedUtils::DrawCurrentItemChrome(rowIsSelectedAfterInteraction, ImGui::IsItemHovered(), false, true);

                const bool openContextFromNav = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false);
                if (openContextFromNav) {
                    if (!selected.contains(entry.formID)) {
                        state.selection.Single(entry.formID);
                        trackRecentRecord(entry);
                    }
                    ImGui::OpenPopup("RowContext");
                }

                selectRowOnRightClick();
                ImGui::OpenPopupOnItemClick("RowContext", ImGuiPopupFlags_MouseButtonRight);

                ImGui::TableSetColumnIndex(1);
                const auto* displayName = entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : entry.name.c_str();
                if (isFavoriteRow) {
                    ImGui::Text("â˜… %s", displayName);
                } else {
                    ImGui::TextUnformatted(displayName);
                }
                selectRowOnRightClick();
                ImGui::OpenPopupOnItemClick("RowContext", ImGuiPopupFlags_MouseButtonRight);

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s: %s", L("General", "sName", "Name"), displayName);
                    if (entry.hasNPCData) {
                        const auto* unknown = L("General", "sUnknown", "Unknown");
                        const auto* none = L("General", "sNone", "None");
                        const auto* yes = L("General", "sYes", "Yes");
                        const auto* no = L("General", "sNo", "No");
                        ImGui::Text("%s: %s", L("NPCs", "sFaction", "Faction"), entry.factions.empty() ? none : entry.factions.c_str());
                        ImGui::Text("%s: %s", L("NPCs", "sResolvedRace", "Resolved Race"), entry.race.empty() ? unknown : entry.race.c_str());
                        ImGui::Text("%s: %s", L("NPCs", "sEssential", "Essential"), entry.npcEssential ? yes : no);
                        ImGui::Text("%s: %s", L("NPCs", "sUnique", "Unique"), entry.npcUnique ? yes : no);
                        ImGui::Text("%s: %s", L("NPCs", "sProtected", "Protected"), entry.npcProtected ? yes : no);
                        ImGui::Text("%s: %s", L("NPCs", "sGender", "Gender"), entry.npcFemale ? L("NPCs", "sFemale", "Female") : L("NPCs", "sMale", "Male"));
                    } else {
                        ImGui::Text("%s: %s", L("General", "sCategory", "Category"), entry.category.c_str());
                    }
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
                ImGui::OpenPopupOnItemClick("RowContext", ImGuiPopupFlags_MouseButtonRight);

                if (ImGui::BeginPopup("RowContext")) {
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
                            trackRecentRecord(entry);
                        }
                    }

                    ImGui::Separator();

                    if (multipleSelected) {
                        const auto selectedEntries = collectSelectedEntries();

                        if (!gameplayActionsAllowed || !primaryAllowed()) {
                            ImGui::BeginDisabled(true);
                        }
                        if ((primaryAction || bulkPrimaryAction) && !config.disableBulkPrimaryAction) {
                            const char* actionLabel = config.primaryActionLabel ? config.primaryActionLabel : L("General", "sAction", "Action");
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
                        if (!gameplayActionsAllowed || !primaryAllowed()) {
                            ImGui::EndDisabled();
                        }

                        if (!selectedEntries.empty()) {
                            if (ImGui::MenuItem(CopyLabel(L("General", "sCopyFormID", "Copy FormID"), selectedEntries.size(), "CopyIDs").c_str())) {
                                std::vector<std::string> values{};
                                values.reserve(selectedEntries.size());
                                for (const auto& selectedEntry : selectedEntries) {
                                    values.push_back(FormatUtils::FormID(selectedEntry.formID));
                                }
                                const std::string clipboard = FormatUtils::MultiCopyList(values, config.copyFormat);
                                ImGui::SetClipboardText(clipboard.c_str());
                            }

                            if (ImGui::MenuItem(CopyLabel(L("General", "sCopyName", "Copy Name"), selectedEntries.size(), "CopyNames").c_str())) {
                                std::vector<std::string> values{};
                                values.reserve(selectedEntries.size());
                                for (const auto& selectedEntry : selectedEntries) {
                                    values.push_back(selectedEntry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : selectedEntry.name);
                                }
                                const std::string clipboard = FormatUtils::MultiCopyList(values, config.copyFormat);
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

                    if (actions.rowContext) {
                        actions.rowContext(entry, multipleSelected);
                    }

                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
        }

        ImGui::EndTable();
        ImGui::PopStyleColor(4);

        ImGui::TextDisabled("%s: %zu  |  %s: %zu", L("General", "sVisible", "Visible"), entries.size(), L("General", "sSelectedShort", "Sel"), selected.size());

        const FormEntry* selectedEntry = nullptr;
        if (state.selection.active && selected.contains(state.selection.active)) {
            selectedEntry = &entries[state.displayedPositions.at(state.selection.active)];
        }
        if (!selectedEntry && !selected.empty()) {
            auto position = entries.size();
            for (auto id : selected) position = (std::min)(position, state.displayedPositions.at(id));
            selectedEntry = &entries[position];
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
                const bool disableThisAction = !gameplayActionsAllowed || !primaryAllowed() || (multipleSelected && config.disableBulkPrimaryAction);
                const char* label = config.primaryActionLabel ? config.primaryActionLabel : L("General", "sAction", "Action");
                std::string actionLabel = multipleSelected
                    ? std::string(label) + " (" + std::to_string(selected.size()) + ")"
                    : std::string(label);

                if (disableThisAction) {
                    ImGui::BeginDisabled(true);
                }
                if (drawWrappedButton((actionLabel + "###SelectedAction").c_str(), firstSelectedActionInRow)) {
                    if (multipleSelected) {
                        invokePrimaryForSelection();
                    } else if (primaryAction) {
                        primaryAction(*selectedEntry);
                    } else {
                        invokePrimaryForSelection();
                    }
                }
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                if (disableThisAction) {
                    ImGui::EndDisabled();
                }
            }

            if (quantityAction) {
                const char* qtyLabel = config.quantityActionLabel ? config.quantityActionLabel : L("NPCs", "sSpawnAtPlayer", "Spawn At Player");
                std::string qtyActionLabel = multipleSelected
                    ? std::string(qtyLabel) + " (" + std::to_string(selected.size()) + ")"
                    : std::string(qtyLabel);

                if (!gameplayActionsAllowed || !primaryAllowed()) {
                    ImGui::BeginDisabled(true);
                }
                if (drawWrappedButton((qtyActionLabel + "###SelectedQuantityAction").c_str(), firstSelectedActionInRow)) {
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
                ImGuiWidgetUtils::ShowGameplayDisabledTooltip(gameplayActionsAllowed, disabledTooltip);
                ImGui::SameLine();
                ImVec4 selectedFrameBg = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
                ImVec4 selectedFrameBgHovered = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered);
                ImVec4 selectedFrameBgActive = ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive);
                selectedFrameBg.w = (std::max)(selectedFrameBg.w, 0.30f);
                selectedFrameBgHovered.w = (std::max)(selectedFrameBgHovered.w, 0.36f);
                selectedFrameBgActive.w = (std::max)(selectedFrameBgActive.w, 0.42f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, selectedFrameBg);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, selectedFrameBgHovered);
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, selectedFrameBgActive);
                ImGui::SetNextItemWidth(140.0f);
                ImGui::InputInt("##SelectedQty", &pendingQuantity, 1, 10);
                ImGui::PopStyleColor(3);
                if (!gameplayActionsAllowed || !primaryAllowed()) {
                    ImGui::EndDisabled();
                }
                if (pendingQuantity < 1) {
                    pendingQuantity = 1;
                }
                firstSelectedActionInRow = false;
            }

            if (config.allowFavorites && favorites) {
                if (multipleSelected) {
                        if (drawWrappedButton(L("General", "sToggleFavoritesSelected", "Toggle Favorites (Selected)"), firstSelectedActionInRow)) {
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
                    ImGui::SetClipboardText(FormatUtils::FormID(selectedEntry->formID).c_str());
                }
            }
        }

        ImGui::PopID();
    }

}
