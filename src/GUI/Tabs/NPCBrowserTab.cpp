#include "GUI/Tabs/NPCBrowserTab.h"
#include "Core/RecordActions.h"
#include "GUI/Widgets/BrowserWidgets.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        void DrawSearchableCombo(
            const char* comboId,
            const char* searchHint,
            const char* allLabel,
            const char* unknownLabel,
            std::optional<std::string>& selected,
            char* searchBuffer,
            std::size_t searchBufferSize,
            bool& dropdownJustOpened,
            const std::vector<std::string>& items,
            const std::function<std::size_t(const std::string&)>& getCount,
            float comboWidth)
        {
            std::string preview = !selected ? allLabel : (selected->empty() ? unknownLabel : *selected);
            if (selected) {
                preview += " (" + std::to_string(getCount(*selected)) + ")";
            }

            ImGui::SetNextItemWidth(comboWidth);
            ImGui::SetNextWindowSizeConstraints(ImVec2(comboWidth, 200.0f), ImVec2(comboWidth, 600.0f));
            if (ImGui::BeginCombo(comboId, preview.c_str(), ImGuiComboFlags_HeightLargest)) {
                if (!dropdownJustOpened) {
                    searchBuffer[0] = '\0';
                    dropdownJustOpened = true;
                    ImGui::SetKeyboardFocusHere();
                }

                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputTextWithHint("##Search", searchHint, searchBuffer, searchBufferSize);

                if (dropdownJustOpened) {
                    ImGui::SetItemDefaultFocus();
                }

                ImGui::Separator();

                if (ImGui::Selectable((std::string(allLabel) + "###All").c_str(), !selected)) {
                    selected.reset();
                    dropdownJustOpened = false;
                }

                const std::string_view filter{ searchBuffer };
                for (const auto& item : items) {
                    const char* displayName = item.empty() ? unknownLabel : item.c_str();

                    if (!TextContains(displayName, filter)) continue;

                    const std::size_t count = getCount(item);
                    const std::string label = std::string(displayName) + " (" + std::to_string(count) + ")###" + item;
                    if (ImGui::Selectable(label.c_str(), selected == item)) {
                        selected = item;
                        dropdownJustOpened = false;
                    }
                }

                ImGui::EndCombo();
            } else {
                dropdownJustOpened = false;
            }
        }
    }

    void NPCBrowserTab::Draw(NPCBrowserState& state, const BrowserView& view, BrowserRequests& requests)
    {
        state.browser.activeCategory = "NPC_";
        BrowserWidgets::DrawControls(state.browser, view, requests, "NPCBrowser", "NPCs", "NPC Search");
        auto& rows = state.browser.categories["NPC_"];
        auto baseQuery = BrowserWidgets::MakeQuery(state.browser, view, rows, "NPC_");
        // Facet counts keep the existing global-filter scope, independent of the
        // selected race/faction, text search and plugin narrowing.
        auto facetQuery = baseQuery;
        facetQuery.search.clear();
        facetQuery.plugin.clear();
        facetQuery.sortColumn = 0;
        facetQuery.ascending = true;
        const auto& facetResult = state.facetQuery.Update(view.catalog, facetQuery, view.filters.advancedRecordFilters, view.filters.advancedRecordFilterRevision);
        const ResultRevision facetRevision{ view.catalog->generation, facetResult.revision };
        if (state.facetRevision != facetRevision) {
            state.facets = NPCFacets::Build(facetResult);
            state.facetRevision = facetRevision;
        }
        const auto& localize = view.localize;
        auto& filterState = state.filters;
        const auto& derivedData = state.facets;
        struct FilterModeEntry { NPCFilterMode mode; const char* label; };
        const FilterModeEntry filterModes[] = {
            { NPCFilterMode::kAll,        localize("General", "sAll", "") },
            { NPCFilterMode::kByRace,     localize("NPCs", "sResolvedRace", "") },
            { NPCFilterMode::kByFaction,  localize("NPCs", "sFaction", "") },
            { NPCFilterMode::kEssential,  localize("NPCs", "sEssential", "") },
            { NPCFilterMode::kUnique,     localize("NPCs", "sUnique", "") },
            { NPCFilterMode::kProtected,  localize("NPCs", "sProtected", "") },
            { NPCFilterMode::kMaleOnly,   localize("NPCs", "sMale", "") },
            { NPCFilterMode::kFemaleOnly, localize("NPCs", "sFemale", "") },
        };

        const char* currentModeLabel = localize("General", "sAll", "");
        for (const auto& fm : filterModes) {
            if (fm.mode == filterState.mode) {
                currentModeLabel = fm.label;
                break;
            }
        }

        const float filterComboWidth = (std::min)(180.0f, ImGui::GetContentRegionAvail().x * 0.2f);
        ImGui::SetNextItemWidth(filterComboWidth);
        if (ImGui::BeginCombo("##NPCFilterMode", currentModeLabel)) {
            for (const auto& fm : filterModes) {
                if (ImGui::Selectable(fm.label, filterState.mode == fm.mode)) {
                    filterState.mode = fm.mode;
                    filterState.selectedRace.reset();
                    filterState.selectedFaction.reset();
                }
            }
            ImGui::EndCombo();
        }

        if (filterState.mode == NPCFilterMode::kByRace) {
            ImGui::SameLine();
            const float subComboWidth = (std::min)(320.0f, (std::max)(200.0f, ImGui::GetContentRegionAvail().x * 0.5f));
            DrawSearchableCombo(
                "##NPCRaceFilter",
                localize("General", "sSearch", ""),
                localize("General", "sAll", ""),
                localize("General", "sUnknown", ""),
                filterState.selectedRace,
                state.raceSearchBuffer,
                sizeof(state.raceSearchBuffer),
                state.raceDropdownJustOpened,
                derivedData.races,
                [&](const std::string& race) -> std::size_t {
                    auto it = derivedData.raceCounts.find(race);
                    return it != derivedData.raceCounts.end() ? it->second : 0;
                },
                subComboWidth);
        }

        if (filterState.mode == NPCFilterMode::kByFaction) {
            ImGui::SameLine();
            const float subComboWidth = (std::min)(320.0f, (std::max)(200.0f, ImGui::GetContentRegionAvail().x * 0.5f));
            DrawSearchableCombo(
                "##NPCFactionFilter",
                localize("General", "sSearch", ""),
                localize("General", "sAll", ""),
                localize("General", "sUnknown", ""),
                filterState.selectedFaction,
                state.factionSearchBuffer,
                sizeof(state.factionSearchBuffer),
                state.factionDropdownJustOpened,
                derivedData.factions,
                [&](const std::string& faction) -> std::size_t {
                    auto it = derivedData.factionCounts.find(faction);
                    return it != derivedData.factionCounts.end() ? it->second : 0;
                },
                subComboWidth);
        }

        ImGui::Separator();

        baseQuery.npc = state.filters;
        const auto& result = rows.query.Update(view.catalog, baseQuery, view.filters.advancedRecordFilters, view.filters.advancedRecordFilterRevision);
        ImGui::TextDisabled("%zu %s", result.order.size(), localize("NPCs", "sResults", "NPCs"));
        if (filterState.mode != NPCFilterMode::kAll) {
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string(localize("General", "sClearFilter", "Clear Filter")) + "###ClearNPCFilter").c_str())) state.filters = {};
        }
        const FormTableConfig config{
            .tableId = "NPCTable", .primaryActionLabel = localize("NPCs", "sSpawnNPC", "Spawn"),
            .allowFavorites = true, .gameplayActionsAllowed = view.gameplayReady, .copyFormat = view.copyFormat, .doubleClickGameplayAction = view.doubleClickGameplayAction, .compactDensity = view.compactTableDensity
        };
        const FormTableActions actions{
            .primary = [&](const FormEntry& entry) { BrowserWidgets::Emit(requests, view, entry, ActionKind::Spawn); },
            .rowContext = [&](const FormEntry& entry, bool multiple) { BrowserWidgets::DrawContext(entry, multiple ? BrowserWidgets::ContextScope::Selection : BrowserWidgets::ContextScope::Single, rows.contextQuantities, view, requests); },
            .canPrimary = [](const FormEntry& entry) { return !entry.isDeleted && SupportsRecordAction(entry.category, ActionKind::Spawn); },
            .selected = [&](auto id) { requests.recentSelections.push_back(id); },
            .inspect = [&](auto id) { requests.inspections.push_back(id); },
            .basket = [&](const auto& entries) { for (const auto& entry : entries) requests.basket.push_back(entry.formID); },
            .collect = [&](const auto& entries) { for (const auto& entry : entries) requests.collections.push_back(entry.formID); }
        };
        FormTable::DrawPrepared(rows.table, result, config, actions, &view.favorites);
    }
}
