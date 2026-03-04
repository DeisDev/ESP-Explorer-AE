#include "GUI/Tabs/NPCBrowserTab.h"

#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/FormTable.h"
#include "GUI/Widgets/RecordFiltersWidget.h"
#include "GUI/Widgets/SearchBar.h"

#include <RE/T/TESActorBaseData.h>
#include <RE/T/TESFaction.h>
#include <RE/T/TESNPC.h>

#include <imgui.h>

namespace ESPExplorerAE
{
    namespace
    {
        enum class NPCFilterMode : int
        {
            kAll = 0,
            kByRace,
            kByFaction,
            kEssential,
            kUnique,
            kProtected,
            kMaleOnly,
            kFemaleOnly
        };

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

        struct NPCFilterState
        {
            NPCFilterMode mode{ NPCFilterMode::kAll };
            std::string selectedRace{};
            std::string selectedFaction{};
            char raceSearchBuffer[128]{};
            char factionSearchBuffer[128]{};
            bool raceDropdownJustOpened{ false };
            bool factionDropdownJustOpened{ false };
        };

        const std::vector<FormEntry>& GetFilteredEntries(
            LocalFilterCache& cacheState,
            const std::vector<FormEntry>& source,
            bool showPlayable,
            bool showNonPlayable,
            bool showNamed,
            bool showUnnamed,
            bool showDeleted,
            const NPCBrowserTab::FilterEntriesFn& filterEntries)
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

        std::string GetFactionDisplayName(const RE::TESFaction* faction)
        {
            if (!faction) {
                return {};
            }

            const auto* factionName = faction->GetFullName();
            if (factionName && factionName[0] != '\0') {
                return factionName;
            }

            const auto* factionEditorID = faction->GetFormEditorID();
            if (factionEditorID && factionEditorID[0] != '\0') {
                return factionEditorID;
            }

            return {};
        }

        struct NPCDerivedData
        {
            std::vector<std::string> races;
            std::unordered_map<std::string, std::size_t> raceCounts;
            std::vector<std::string> factions;
            std::unordered_map<std::string, std::vector<FormEntry>> factionEntries;
        };

        NPCDerivedData BuildDerivedData(const std::vector<FormEntry>& filteredNPCs)
        {
            NPCDerivedData data;

            std::unordered_set<std::string> seenRaces;
            seenRaces.reserve(filteredNPCs.size());

            for (const auto& entry : filteredNPCs) {
                if (seenRaces.insert(entry.category).second) {
                    data.races.push_back(entry.category);
                }
                data.raceCounts[entry.category]++;
            }

            std::ranges::sort(data.races, [](const std::string& left, const std::string& right) {
                if (left.empty()) return false;
                if (right.empty()) return true;
                return left < right;
            });

            data.factionEntries.reserve(filteredNPCs.size());
            for (const auto& entry : filteredNPCs) {
                auto* npcForm = RE::TESForm::GetFormByID(entry.formID) ? RE::TESForm::GetFormByID(entry.formID)->As<RE::TESNPC>() : nullptr;
                if (!npcForm) continue;

                std::unordered_set<std::string> addedFactionLabels;
                for (const auto& factionRank : npcForm->factions) {
                    if (!factionRank.faction || factionRank.rank < 0) continue;
                    const std::string factionLabel = GetFactionDisplayName(factionRank.faction);
                    if (factionLabel.empty() || !addedFactionLabels.insert(factionLabel).second) continue;
                    data.factionEntries[factionLabel].push_back(entry);
                }
            }

            data.factions.reserve(data.factionEntries.size());
            for (const auto& [label, _] : data.factionEntries) {
                data.factions.push_back(label);
            }
            std::ranges::sort(data.factions);

            return data;
        }

        bool NPCPassesAttributeFilter(const FormEntry& entry, NPCFilterMode mode)
        {
            auto* npcForm = RE::TESForm::GetFormByID(entry.formID) ? RE::TESForm::GetFormByID(entry.formID)->As<RE::TESNPC>() : nullptr;
            if (!npcForm) return false;

            switch (mode) {
            case NPCFilterMode::kEssential:
                return npcForm->IsEssential();
            case NPCFilterMode::kUnique:
                return npcForm->IsUnique();
            case NPCFilterMode::kProtected:
                return npcForm->IsProtected();
            case NPCFilterMode::kMaleOnly:
                return !npcForm->IsFemale();
            case NPCFilterMode::kFemaleOnly:
                return npcForm->IsFemale();
            default:
                return true;
            }
        }

        void DrawSearchableCombo(
            const char* comboId,
            const char* searchHint,
            const char* allLabel,
            const char* unknownLabel,
            std::string& selected,
            char* searchBuffer,
            std::size_t searchBufferSize,
            bool& dropdownJustOpened,
            const std::vector<std::string>& items,
            const std::function<std::size_t(const std::string&)>& getCount,
            float comboWidth)
        {
            std::string preview = selected.empty() ? allLabel : selected;
            if (!selected.empty()) {
                preview += " (" + std::to_string(getCount(selected)) + ")";
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

                if (ImGui::Selectable(allLabel, selected.empty())) {
                    selected.clear();
                    dropdownJustOpened = false;
                }

                const std::string_view filter{ searchBuffer };
                for (const auto& item : items) {
                    const char* displayName = item.empty() ? unknownLabel : item.c_str();

                    if (!filter.empty()) {
                        std::string displayLower(displayName);
                        std::string filterLower(filter);
                        std::ranges::transform(displayLower, displayLower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                        std::ranges::transform(filterLower, filterLower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                        if (displayLower.find(filterLower) == std::string::npos) continue;
                    }

                    const std::size_t count = getCount(item);
                    const std::string label = std::string(displayName) + " (" + std::to_string(count) + ")";
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

    void NPCBrowserTab::Draw(
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
                "NPCBrowser",
                RecordFilterState{
                    .showNonPlayable = showNonPlayableRecords,
                    .showUnnamed = showUnnamedRecords,
                    .showDeleted = showDeletedRecords })) {
            persistListFilters();
        }

        SearchBar::Draw(localize("NPCs", "sSearch", "NPC Search"), searchBuffer, searchBufferSize, searchText, searchFocusPending);
        drawPluginFilterStatus();
        ImGui::Separator();

        const FormTableConfig tableConfig{
            .tableId = "NPCTable",
            .primaryActionLabel = localize("NPCs", "sSpawnNPC", "Spawn"),
            .quantityActionLabel = nullptr,
            .allowFavorites = true
        };

        static LocalFilterCache npcFilterCache{};
        const auto& filteredNPCs = GetFilteredEntries(
            npcFilterCache,
            cache.npcs,
            showPlayableRecords,
            showNonPlayableRecords,
            showNamedRecords,
            showUnnamedRecords,
            showDeletedRecords,
            filterEntries);

        static NPCFilterState filterState{};

        static NPCDerivedData derivedData{};
        static const std::vector<FormEntry>* lastSource{ nullptr };
        static std::size_t lastSourceSize{ 0 };
        if (lastSource != &filteredNPCs || lastSourceSize != filteredNPCs.size()) {
            derivedData = BuildDerivedData(filteredNPCs);
            lastSource = &filteredNPCs;
            lastSourceSize = filteredNPCs.size();
        }

        struct FilterModeEntry { NPCFilterMode mode; const char* label; };
        const FilterModeEntry filterModes[] = {
            { NPCFilterMode::kAll,        localize("General", "sAll", "") },
            { NPCFilterMode::kByRace,     localize("General", "sRace", "") },
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
                    filterState.selectedRace.clear();
                    filterState.selectedFaction.clear();
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
                filterState.raceSearchBuffer,
                sizeof(filterState.raceSearchBuffer),
                filterState.raceDropdownJustOpened,
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
                filterState.factionSearchBuffer,
                sizeof(filterState.factionSearchBuffer),
                filterState.factionDropdownJustOpened,
                derivedData.factions,
                [&](const std::string& faction) -> std::size_t {
                    auto it = derivedData.factionEntries.find(faction);
                    return it != derivedData.factionEntries.end() ? it->second.size() : 0;
                },
                subComboWidth);
        }

        ImGui::Separator();

        std::vector<FormEntry> displayEntries;

        switch (filterState.mode) {
        case NPCFilterMode::kAll:
            displayEntries = filteredNPCs;
            break;

        case NPCFilterMode::kByRace:
            if (filterState.selectedRace.empty()) {
                displayEntries = filteredNPCs;
            } else {
                displayEntries.reserve(filteredNPCs.size() / 4);
                for (const auto& entry : filteredNPCs) {
                    if (entry.category == filterState.selectedRace) {
                        displayEntries.push_back(entry);
                    }
                }
            }
            break;

        case NPCFilterMode::kByFaction:
            if (filterState.selectedFaction.empty()) {
                displayEntries = filteredNPCs;
            } else {
                auto it = derivedData.factionEntries.find(filterState.selectedFaction);
                if (it != derivedData.factionEntries.end()) {
                    displayEntries = it->second;
                }
            }
            break;

        case NPCFilterMode::kEssential:
        case NPCFilterMode::kUnique:
        case NPCFilterMode::kProtected:
        case NPCFilterMode::kMaleOnly:
        case NPCFilterMode::kFemaleOnly:
            displayEntries.reserve(filteredNPCs.size() / 4);
            for (const auto& entry : filteredNPCs) {
                if (NPCPassesAttributeFilter(entry, filterState.mode)) {
                    displayEntries.push_back(entry);
                }
            }
            break;
        }

        ImGui::TextDisabled("%zu %s", displayEntries.size(), localize("NPCs", "sResults", ""));
        ImGui::SameLine();

        if (filterState.mode != NPCFilterMode::kAll) {
            ImGui::SameLine();
            if (ImGui::SmallButton(localize("General", "sClearFilter", "Clear Filter"))) {
                filterState.mode = NPCFilterMode::kAll;
                filterState.selectedRace.clear();
                filterState.selectedFaction.clear();
            }
        }

        FormTable::Draw(
            displayEntries,
            searchText,
            selectedPluginFilter,
            tableConfig,
            [](const FormEntry& entry) {
                FormActions::SpawnAtPlayer(entry.formID, 1);
            },
            {},
            [](const FormEntry& entry, int quantity) {
                FormActions::SpawnAtPlayer(entry.formID, static_cast<std::uint32_t>(quantity));
            },
            &favoriteForms,
            contextCallbacks);
    }
}
