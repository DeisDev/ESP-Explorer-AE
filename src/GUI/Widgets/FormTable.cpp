#include "GUI/Widgets/FormTable.h"

#include "Core/Actions.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/SharedUtils.h"
#include "Localization/Language.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <array>
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
        struct ColumnLabel { const char* section; const char* key; const char* fallback; };
        constexpr ColumnLabel labels[]{
            ColumnLabel{"General", "sFormID", "FormID"}, {"General", "sName", "Name"}, {"General", "sPlugin", "Source"},
            {"General", "sType", "Type"}, {"General", "sEditorID", "EditorID"}, {"Items", "sAmmo", "Ammunition"},
            {"FormDetails", "sDamageBase", "Base Damage"}, {"General", "sWeight", "Weight"}, {"General", "sValue", "Value"},
            {"NPCs", "sResolvedRace", "Race"}, {"NPCs", "sEssential", "Essential"}, {"NPCs", "sUnique", "Unique"},
            {"NPCs", "sProtected", "Protected"}, {"FormDetails", "sInterior", "Interior"}, {"FormDetails", "sWorldSpace", "Worldspace"}
        };
        const char* ColumnName(int column) { const auto& label = labels[column]; return L(label.section, label.key, label.fallback); }
        class TableText
        {
            std::array<ImGuiLocEntry, 4> previous;
        public:
            TableText()
            {
                const std::array values{
                    ImGuiLocEntry{ImGuiLocKey_TableSizeOne, L("Inventory", "sFitColumn", "Size Column to Fit")},
                    ImGuiLocEntry{ImGuiLocKey_TableSizeAllFit, L("Inventory", "sFitAllColumns", "Size All Columns to Fit")},
                    ImGuiLocEntry{ImGuiLocKey_TableResetOrder, L("Inventory", "sResetColumnOrder", "Reset Column Order")},
                    ImGuiLocEntry{ImGuiLocKey_TableSizeAllDefault, L("Inventory", "sResetColumnWidths", "Reset Column Widths")}
                };
                for (std::size_t i = 0; i < values.size(); ++i) previous[i] = {values[i].Key, ImGui::LocalizeGetMsg(values[i].Key)};
                ImGui::LocalizeRegisterEntries(values.data(), static_cast<int>(values.size()));
            }
            ~TableText() { ImGui::LocalizeRegisterEntries(previous.data(), static_cast<int>(previous.size())); }
        };
    }

    void FormTable::DrawPrepared(FormTableState& state, const CatalogResult& result, const FormTableConfig& config,
        const FormTableActions& actions, std::unordered_set<std::uint32_t>* favorites)
    {
        const ResultRevision revision{ result.snapshot ? result.snapshot->generation : 0, result.revision };
        if (state.orderRevision != revision) {
            state.displayedIDs.clear(); state.displayedPositions.clear();
            for (const auto index : result.order) {
                const auto id = result.snapshot->records[index].formID;
                state.displayedPositions.emplace(id, state.displayedIDs.size());
                state.displayedIDs.push_back(id);
            }
            state.selection.Reconcile(state.displayedIDs);
            state.orderRevision = revision;
        }
        ImGui::PushID(config.tableId);
        const auto selectedEntries = [&] {
            std::vector<FormEntry> entries;
            for (std::size_t row = 0; row < result.order.size(); ++row)
                if (state.selection.selected.contains(result.At(row).formID)) entries.push_back(result.At(row));
            return entries;
        };
        const auto canAct = [&] {
            if (!config.gameplayActionsAllowed || state.selection.selected.empty()) return false;
            return std::ranges::all_of(state.selection.selected, [&](auto id) {
                const auto* record = result.snapshot->Find(id);
                return record && !record->isDeleted && (!actions.canPrimary || actions.canPrimary(*record));
            });
        };
        const auto primary = [&] {
            const auto entries = selectedEntries();
            if (actions.bulkPrimary) actions.bulkPrimary(entries);
            else if (actions.primary) for (const auto& entry : entries) actions.primary(entry);
        };
        const auto copy = [&](bool names) {
            std::vector<std::string> values;
            for (const auto& entry : selectedEntries()) values.push_back(names ? entry.name : FormatUtils::FormID(entry.formID));
            ImGui::SetClipboardText(FormatUtils::MultiCopyList(values, config.copyFormat).c_str());
        };
        const auto menu = [&] {
            if (ImGui::MenuItem(L("General", "sSelectVisible", "Select Visible"))) state.selection.All(state.displayedIDs);
            if (ImGui::MenuItem(L("General", "sClearSelection", "Clear Selection"))) state.selection.Clear();
            ImGui::Separator();
            ImGui::BeginDisabled(state.selection.selected.empty());
            if (ImGui::MenuItem(L("General", "sCopyFormID", "Copy FormID"))) copy(false);
            if (ImGui::MenuItem(L("General", "sCopyName", "Copy Name"))) copy(true);
            if (actions.basket && ImGui::MenuItem(L("Workspace", "sAddToBasket", "Add to Basket"))) actions.basket(selectedEntries());
            if (actions.collect && ImGui::MenuItem(L("General", "sAddToCollection", "Add to Collection"))) actions.collect(selectedEntries());
            if (favorites && config.allowFavorites && ImGui::MenuItem(L("General", "sToggleFavoritesSelected", "Toggle Favorites (Selected)"))) {
                for (auto id : state.selection.selected) if (!favorites->erase(id)) favorites->insert(id);
            }
            ImGui::EndDisabled();
            ImGui::Separator();
            ImGui::BeginDisabled(!canAct());
            if (actions.bulkSecondary && config.secondaryActionLabel && ImGui::MenuItem(config.secondaryActionLabel)) actions.bulkSecondary(selectedEntries());
            if (actions.quantity) {
                ImGui::SetNextItemWidth(160);
                ImGui::InputInt(L("General", "sQuantity", "Quantity"), &state.bulkQuantity);
                state.bulkQuantity = std::clamp(state.bulkQuantity, 1, static_cast<int>(ActionQueue::MaxQuantity));
                if (ImGui::MenuItem(config.quantityActionLabel ? config.quantityActionLabel : L("NPCs", "sSpawnAtPlayer", "Spawn At Player")))
                    for (const auto& entry : selectedEntries()) actions.quantity(entry, state.bulkQuantity);
            }
            ImGui::EndDisabled();
        };
        // Fixed toolbar and footer reserve the same space regardless of selection.
        if (ImGui::Button(L("General", "sActions", "Actions"))) ImGui::OpenPopup("TableActions");
        if (ImGui::BeginPopup("TableActions")) { menu(); ImGui::EndPopup(); }
        ImGui::SameLine();
        if (ImGui::Button(L("General", "sColumns", "Columns"))) ImGui::OpenPopup("TableColumns");
        if (ImGui::BeginPopup("TableColumns")) {
            if (ImGui::MenuItem(L("General", "sStandardColumns", "Standard columns"))) { state.columns = DefaultRecordColumns(); state.restoreColumns = true; }
            if (ImGui::MenuItem(L("General", "sTechnicalColumns", "Technical columns"))) { state.columns = DefaultRecordColumns(true); state.restoreColumns = true; }
            ImGui::Separator();
            for (int column = 0; column < static_cast<int>(RecordColumn::Count); ++column) {
                if (ImGui::MenuItem(ColumnName(column), nullptr, state.columns[column].visible, column != 1)) {
                    state.columns[column].visible = !state.columns[column].visible;
                    state.restoreColumns = true;
                }
            }
            ImGui::EndPopup();
        }
        if (actions.primary || actions.bulkPrimary) {
            ImGui::SameLine();
            ImGui::BeginDisabled(!canAct() || (config.disableBulkPrimaryAction && state.selection.selected.size() > 1));
            const auto label = std::string(config.primaryActionLabel ? config.primaryActionLabel : L("General", "sAction", "Action")) + "###BulkAction";
            if (ImGui::Button(label.c_str())) primary();
            ImGui::EndDisabled();
        }
        const float height = (std::max)(ImGui::GetFrameHeight() * 2, ImGui::GetContentRegionAvail().y - ImGui::GetTextLineHeightWithSpacing());
        const TableText localizedTableText;
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {ImGui::GetStyle().CellPadding.x, config.compactDensity ? 1.0f : 5.0f});
        if (state.restoreScroll) ImGui::SetNextWindowScroll({0.0f, state.scroll});
        constexpr auto flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH |
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_NoSavedSettings;
        if (ImGui::BeginTable(config.tableId, static_cast<int>(RecordColumn::Count), flags, {0, height})) {
            if (ImGui::IsWindowFocused() && !ImGui::GetIO().WantTextInput &&
                !ImGui::IsAnyItemActive() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) state.selection.All(state.displayedIDs);
                if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false)) copy(false);
            }
            state.restoreScroll = false;
            state.scroll = ImGui::GetScrollY();
            for (int column = 0; column < static_cast<int>(RecordColumn::Count); ++column) {
                ImGuiTableColumnFlags columnFlags = ImGuiTableColumnFlags_WidthFixed;
                if (!state.columns[column].visible) columnFlags |= ImGuiTableColumnFlags_DefaultHide;
                if (column == 1) columnFlags |= ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_NoHide;
                ImGui::TableSetupColumn(ColumnName(column), columnFlags, state.columns[column].width, static_cast<ImGuiID>(column + 1));
            }
            auto* table = ImGui::GetCurrentTable();
            if (table->IsInitializing) state.restoreColumns = true;
            if (state.restoreColumns) {
                for (int order = 0; order < static_cast<int>(RecordColumn::Count); ++order)
                    for (int column = 0; column < static_cast<int>(RecordColumn::Count); ++column) if (state.columns[column].order == order)
                        ImGui::TableSetColumnDisplayOrder(table, column, order);
                for (int column = 0; column < static_cast<int>(RecordColumn::Count); ++column) {
                    ImGui::TableSetColumnEnabled(column, state.columns[column].visible);
                    ImGui::TableSetColumnWidth(column, state.columns[column].width);
                }
                ImGui::TableSetColumnSortDirection(state.sort.column, state.sort.ascending ? ImGuiSortDirection_Ascending : ImGuiSortDirection_Descending, false);
                state.restoreColumns = false;
            }
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            if (auto* sort = ImGui::TableGetSortSpecs(); sort && sort->SpecsCount > 0 && sort->SpecsDirty) {
                state.sort = { static_cast<int>(sort->Specs[0].ColumnUserID) - 1, sort->Specs[0].SortDirection == ImGuiSortDirection_Ascending };
                sort->SpecsDirty = false;
            }
            for (int column = 0; column < static_cast<int>(RecordColumn::Count); ++column) {
                const auto& value = table->Columns[column];
                state.columns[column] = {value.WidthGiven > 0 ? value.WidthGiven : state.columns[column].width, value.DisplayOrder, value.IsUserEnabledNextFrame};
            }
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(result.order.size()));
            while (clipper.Step()) for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const auto& entry = result.At(static_cast<std::size_t>(row));
                ImGui::PushID(static_cast<int>(entry.formID));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1);
                const auto name = std::string(entry.name.empty() ? L("General", "sUnnamed", "<Unnamed>") : entry.name.c_str()) + "###Record";
                if (ImGui::Selectable(name.c_str(), state.selection.selected.contains(entry.formID), ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap | ImGuiSelectableFlags_AllowDoubleClick)) {
                    state.selection.Click(state.displayedIDs, entry.formID, ImGui::GetIO().KeyCtrl, ImGui::GetIO().KeyShift);
                    if (state.selection.selected.contains(entry.formID) && actions.selected) actions.selected(entry.formID);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if (config.doubleClickGameplayAction && actions.primary && canAct()) actions.primary(entry);
                        else if (actions.inspect) actions.inspect(entry.formID);
                    }
                }
                const bool focused = ImGui::IsItemFocused();
                if (focused && ImGui::IsKeyPressed(ImGuiKey_Enter, false) && actions.inspect) actions.inspect(entry.formID);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || (focused && (ImGui::IsKeyPressed(ImGuiKey_GamepadFaceLeft, false) || (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_F10, false))))) {
                    if (!state.selection.selected.contains(entry.formID)) state.selection.Single(entry.formID);
                    ImGui::OpenPopup("RowContext");
                }
                if (ImGui::BeginPopup("RowContext")) {
                    const bool multiple = state.selection.selected.size() > 1;
                    if (multiple) {
                        ImGui::BeginDisabled(!canAct() || config.disableBulkPrimaryAction);
                        if (ImGui::MenuItem(config.primaryActionLabel ? config.primaryActionLabel : L("General", "sAction", "Action"))) primary();
                        ImGui::EndDisabled();
                        menu(); ImGui::Separator();
                    }
                    if (actions.rowContext) actions.rowContext(entry, multiple);
                    ImGui::EndPopup();
                }
                for (int column = 0; column < static_cast<int>(RecordColumn::Count); ++column) {
                    if (column == 1 || !ImGui::TableSetColumnIndex(column)) continue;
                    if (column == 0) ImGui::TextUnformatted(FormatUtils::FormID(entry.formID).c_str());
                    else if (RecordColumnNumeric(column)) {
                        const auto number = RecordColumnNumber(entry, column);
                        if (!number) ImGui::TextDisabled("%s", L("General", "sUnavailable", "Unavailable"));
                        else if (column >= 10) ImGui::TextUnformatted(*number ? L("General", "sYes", "Yes") : L("General", "sNo", "No"));
                        else ImGui::Text("%g", *number);
                    } else {
                        const auto text = RecordColumnText(entry, column);
                        if (column == 3) {
                            ImGui::TextDisabled("%s%s", entry.category.c_str(), favorites && favorites->contains(entry.formID) ? " *" : "");
                            if (entry.isDeleted && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", L("General", "sDeleted", "Deleted"));
                        } else ImGui::TextUnformatted(text.empty() ? L("General", "sUnavailable", "Unavailable") : text.data(), text.empty() ? nullptr : text.data() + text.size());
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
        ImGui::TextDisabled("%s: %zu | %s: %zu", L("General", "sVisible", "Visible"), result.order.size(), L("General", "sSelectedShort", "Sel"), state.selection.selected.size());
        ImGui::PopID();
    }
}
