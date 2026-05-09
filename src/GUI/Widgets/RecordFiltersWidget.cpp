#include "GUI/Widgets/RecordFiltersWidget.h"

#include "Filters/AdvancedRecordFilters.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/SharedUtils.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <ranges>
#include <unordered_map>

namespace ESPExplorerAE
{
    namespace
    {
        struct AdvancedFilterEditorState
        {
            bool open{ false };
            bool reopenAfterMenuShow{ false };
            bool focusPending{ false };
            int newField{ static_cast<int>(AdvancedFilterField::Any) };
            int newMatch{ static_cast<int>(AdvancedFilterMatch::Contains) };
            char newValue[256]{};
            char keywordSearch[128]{};
            char hiddenPluginSearch[128]{};
            char ruleScopeSearch[128]{};
        };

        auto& GetEditorStates()
        {
            static std::unordered_map<std::string, AdvancedFilterEditorState> states{};
            return states;
        }

        AdvancedFilterEditorState& GetEditorState(std::string_view idSuffix)
        {
            return GetEditorStates()[std::string(idSuffix)];
        }

        const char* FieldLabel(const RecordFiltersWidget::LocalizeFn& localize, AdvancedFilterField field)
        {
            switch (field) {
            case AdvancedFilterField::Any:
                return localize("General", "sAdvancedFilterFieldAny", "Any Field");
            case AdvancedFilterField::Name:
                return localize("General", "sAdvancedFilterFieldName", "Name");
            case AdvancedFilterField::EditorID:
                return localize("General", "sAdvancedFilterFieldEditorID", "EditorID");
            case AdvancedFilterField::Plugin:
                return localize("General", "sAdvancedFilterFieldPlugin", "Plugin");
            case AdvancedFilterField::Category:
                return localize("General", "sAdvancedFilterFieldCategory", "Category");
            case AdvancedFilterField::Keyword:
                return localize("General", "sAdvancedFilterFieldKeyword", "Keyword");
            }

            return "";
        }

        const char* MatchLabel(const RecordFiltersWidget::LocalizeFn& localize, AdvancedFilterMatch match)
        {
            switch (match) {
            case AdvancedFilterMatch::Contains:
                return localize("General", "sAdvancedFilterMatchContains", "Contains");
            case AdvancedFilterMatch::Exact:
                return localize("General", "sAdvancedFilterMatchExact", "Exact");
            case AdvancedFilterMatch::Regex:
                return localize("General", "sAdvancedFilterMatchRegex", "Regex");
            }

            return "";
        }

        const char* ScopeLabel(const RecordFiltersWidget::LocalizeFn& localize, std::string_view idSuffix)
        {
            if (idSuffix == "PluginBrowser") {
                return localize("PluginBrowser", "sBrowserTab", "Plugin Browser");
            }
            if (idSuffix == "ItemBrowser") {
                return localize("Items", "sBrowserTab", "Item Browser");
            }
            if (idSuffix == "NPCBrowser") {
                return localize("NPCs", "sBrowserTab", "NPC Browser");
            }
            if (idSuffix == "CellBrowser") {
                return localize("Cells", "sBrowserTab", "Cell Browser");
            }
            if (idSuffix == "ObjectBrowser") {
                return localize("Objects", "sBrowserTab", "Object Browser");
            }
            if (idSuffix == "SpellPerkBrowser") {
                return localize("Spells", "sBrowserTab", "Spells & Perks");
            }

            return "";
        }

        bool AddRuleIfMissing(std::vector<AdvancedFilterRule>& rules, const AdvancedFilterRule& rule)
        {
            const auto it = std::ranges::find_if(rules, [&](const AdvancedFilterRule& existingRule) {
                return existingRule.field == rule.field &&
                       existingRule.match == rule.match &&
                       existingRule.value == rule.value;
            });
            if (it != rules.end()) {
                const bool changed = !it->enabled;
                it->enabled = true;
                return changed;
            }

            rules.push_back(rule);
            return true;
        }

        bool DrawRuleFieldCombo(const RecordFiltersWidget::LocalizeFn& localize, const char* id, AdvancedFilterField& field)
        {
            bool changed = false;
            if (ImGui::BeginCombo(id, FieldLabel(localize, field))) {
                for (int index = static_cast<int>(AdvancedFilterField::Any); index <= static_cast<int>(AdvancedFilterField::Keyword); ++index) {
                    const auto option = static_cast<AdvancedFilterField>(index);
                    const bool selected = field == option;
                    if (ImGui::Selectable(FieldLabel(localize, option), selected)) {
                        field = option;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            return changed;
        }

        bool DrawRuleMatchCombo(const RecordFiltersWidget::LocalizeFn& localize, const char* id, AdvancedFilterMatch& match)
        {
            bool changed = false;
            if (ImGui::BeginCombo(id, MatchLabel(localize, match))) {
                for (int index = static_cast<int>(AdvancedFilterMatch::Contains); index <= static_cast<int>(AdvancedFilterMatch::Regex); ++index) {
                    const auto option = static_cast<AdvancedFilterMatch>(index);
                    const bool selected = match == option;
                    if (ImGui::Selectable(MatchLabel(localize, option), selected)) {
                        match = option;
                        changed = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            return changed;
        }

        std::string FormatScopeLabel(const RecordFiltersWidget::LocalizeFn& localize, const std::vector<std::string>& targetPlugins)
        {
            if (targetPlugins.empty()) {
                return localize("General", "sAdvancedFilterScopeAll", "All Plugins");
            }
            if (targetPlugins.size() == 1) {
                return targetPlugins[0];
            }
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "%zu %s", targetPlugins.size(), localize("General", "sAdvancedFilterScopePlugins", "plugins"));
            return buf;
        }

        bool DrawRuleScopeCombo(const RecordFiltersWidget::LocalizeFn& localize, const char* id, std::vector<std::string>& targetPlugins, char* searchBuf, std::size_t searchBufSize)
        {
            bool changed = false;
            const auto label = FormatScopeLabel(localize, targetPlugins);
            if (ImGui::BeginCombo(id, label.c_str())) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputTextWithHint("##ScopeSearch", localize("General", "sSearch", "Search..."), searchBuf, searchBufSize);
                ImGui::Separator();

                if (ImGui::Selectable(localize("General", "sAdvancedFilterScopeClear", "-- All Plugins --"), targetPlugins.empty())) {
                    if (!targetPlugins.empty()) {
                        targetPlugins.clear();
                        changed = true;
                    }
                }
                ImGui::Separator();

                const auto& available = AdvancedRecordFilters::GetAvailablePlugins();
                int displayed = 0;
                for (const auto& plugin : available) {
                    if (searchBuf[0] != '\0' && !SharedUtils::ContainsCaseInsensitive(plugin, searchBuf)) {
                        continue;
                    }
                    bool selected = std::ranges::any_of(targetPlugins, [&](const std::string& p) {
                        return SharedUtils::EqualsCaseInsensitive(p, plugin);
                    });
                    if (ImGui::Selectable(plugin.c_str(), selected)) {
                        if (selected) {
                            std::erase_if(targetPlugins, [&](const std::string& p) {
                                return SharedUtils::EqualsCaseInsensitive(p, plugin);
                            });
                        } else {
                            targetPlugins.push_back(plugin);
                        }
                        changed = true;
                    }
                    ++displayed;
                    if (displayed >= 300) break;
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool DrawAdvancedFiltersWindow(const RecordFiltersWidget::LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state, AdvancedFilterEditorState& editorState)
        {
            bool changed = false;

            char windowTitle[160]{};
            const char* scopeLabel = ScopeLabel(localize, idSuffix);
            if (scopeLabel[0] != '\0') {
                std::snprintf(
                    windowTitle,
                    sizeof(windowTitle),
                    "%s - %s###AdvancedFiltersWindow%s",
                    localize("General", "sAdvancedRecordFilters", "Advanced Filters"),
                    scopeLabel,
                    std::string(idSuffix).c_str());
            } else {
                std::snprintf(
                    windowTitle,
                    sizeof(windowTitle),
                    "%s###AdvancedFiltersWindow%s",
                    localize("General", "sAdvancedRecordFilters", "Advanced Filters"),
                    std::string(idSuffix).c_str());
            }

            ImGui::SetNextWindowSize(ImVec2(1060.0f, 620.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(820.0f, 480.0f), ImVec2(2400.0f, 1600.0f));
            if (editorState.focusPending) {
                ImGui::SetNextWindowFocus();
                ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
                editorState.focusPending = false;
            }
            bool windowOpen = true;
            ImGui::Begin(windowTitle, &windowOpen, ImGuiWindowFlags_NoCollapse);
            if (!windowOpen) {
                editorState.open = false;
                ImGui::End();
                return changed;
            }

            SharedUtils::DrawSectionLabel(localize("General", "sAdvancedRecordFilters", "Advanced Filters"));
            ImGui::TextDisabled(
                "%zu %s | %zu %s",
                AdvancedRecordFilters::CountActiveRules(state.advancedRules),
                localize("General", "sAdvancedFiltersActiveSummary", "active block rules"),
                state.hiddenPlugins.size(),
                localize("General", "sAdvancedFiltersHiddenPluginsSummary", "hidden plugins"));
            if (scopeLabel[0] != '\0') {
                ImGui::SameLine();
                ImGui::TextDisabled("| %s", scopeLabel);
            }

            const float totalWidth = ImGui::GetContentRegionAvail().x;
            const float comboWidth = (std::clamp)(totalWidth * 0.18f, 140.0f, 220.0f);
            const float matchWidth = (std::clamp)(totalWidth * 0.14f, 120.0f, 170.0f);
            const float actionWidth = 120.0f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float inputWidth = (std::max)(220.0f, totalWidth - comboWidth - matchWidth - actionWidth - spacing * 3.0f);

            AdvancedFilterField newField = static_cast<AdvancedFilterField>(editorState.newField);
            AdvancedFilterMatch newMatch = static_cast<AdvancedFilterMatch>(editorState.newMatch);

            SharedUtils::DrawSectionLabel(localize("General", "sAdvancedFilterAddRule", "Add Rule"));
            ImGui::SetNextItemWidth(comboWidth);
            if (DrawRuleFieldCombo(localize, ("##AdvancedField" + std::string(idSuffix)).c_str(), newField)) {
                editorState.newField = static_cast<int>(newField);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(matchWidth);
            if (DrawRuleMatchCombo(localize, ("##AdvancedMatch" + std::string(idSuffix)).c_str(), newMatch)) {
                editorState.newMatch = static_cast<int>(newMatch);
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputTextWithHint(
                ("##AdvancedValue" + std::string(idSuffix)).c_str(),
                localize("General", "sAdvancedFilterValueHint", "Value or pattern"),
                editorState.newValue,
                sizeof(editorState.newValue));
            ImGui::SameLine();
            if (ImGui::Button((std::string(localize("General", "sAdvancedFilterAddRule", "Add Rule")) + "##AddRule" + std::string(idSuffix)).c_str(), ImVec2(actionWidth, 0.0f))) {
                if (editorState.newValue[0] != '\0') {
                    const AdvancedFilterRule newRule{
                        .enabled = true,
                        .field = newField,
                        .match = newMatch,
                        .value = editorState.newValue
                    };
                    changed = AddRuleIfMissing(state.advancedRules, newRule) || changed;
                    editorState.newValue[0] = '\0';
                }
            }

            const auto& availableKeywords = AdvancedRecordFilters::GetAvailableKeywords();
            ImGui::SetNextItemWidth((std::min)(380.0f, ImGui::GetContentRegionAvail().x));
            if (ImGui::BeginCombo(
                    (std::string(localize("General", "sAdvancedFilterKeywordPicker", "Add Keyword Rule")) + "##KeywordPicker" + std::string(idSuffix)).c_str(),
                    localize("General", "sAdvancedFilterKeywordPicker", "Add Keyword Rule"))) {
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputTextWithHint(
                    ("##KeywordSearch" + std::string(idSuffix)).c_str(),
                    localize("General", "sSearch", "Search..."),
                    editorState.keywordSearch,
                    sizeof(editorState.keywordSearch));
                ImGui::Separator();

                int displayed = 0;
                for (const auto& keyword : availableKeywords) {
                    if (editorState.keywordSearch[0] != '\0' && !SharedUtils::ContainsCaseInsensitive(keyword, editorState.keywordSearch)) {
                        continue;
                    }

                    if (ImGui::Selectable(keyword.c_str(), false)) {
                        changed = AddRuleIfMissing(state.advancedRules, AdvancedFilterRule{
                            .enabled = true,
                            .field = AdvancedFilterField::Keyword,
                            .match = AdvancedFilterMatch::Exact,
                            .value = keyword
                        }) || changed;
                    }

                    ++displayed;
                    if (displayed >= 250) {
                        break;
                    }
                }

                if (displayed == 0) {
                    ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterNoKeywordResults", "No matching keywords"));
                }

                ImGui::EndCombo();
            }

            bool firstButton = true;
            if (ImGuiWidgetUtils::DrawWrappedButton((std::string(localize("General", "sAdvancedFilterRestoreDefaults", "Restore Defaults")) + "##RestoreDefaults" + std::string(idSuffix)).c_str(), firstButton)) {
                for (const auto& defaultRule : AdvancedRecordFilters::GetDefaultRules()) {
                    changed = AddRuleIfMissing(state.advancedRules, defaultRule) || changed;
                }
            }
            if (ImGuiWidgetUtils::DrawWrappedButton((std::string(localize("General", "sClearAll", "Clear All")) + "##ClearAdvanced" + std::string(idSuffix)).c_str(), firstButton)) {
                state.advancedRules.clear();
                changed = true;
            }

            ImGui::Spacing();

            if (ImGui::BeginChild(("AdvancedRulesRegion" + std::string(idSuffix)).c_str(), ImVec2(0.0f, 0.0f), false)) {
                if (ImGui::CollapsingHeader(
                    (std::string(localize("General", "sHiddenPlugins", "Hidden Plugins")) + " (" + std::to_string(state.hiddenPlugins.size()) + ")###HiddenPluginsSection" + std::string(idSuffix)).c_str(),
                    state.hiddenPlugins.empty() ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen)) {
                    const auto& availablePlugins = AdvancedRecordFilters::GetAvailablePlugins();
                    ImGui::SetNextItemWidth((std::min)(380.0f, ImGui::GetContentRegionAvail().x));
                    if (ImGui::BeginCombo(
                            (std::string(localize("General", "sHidePlugin", "Hide Plugin")) + "##HidePluginPicker" + std::string(idSuffix)).c_str(),
                            localize("General", "sHidePlugin", "Hide Plugin"))) {
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetKeyboardFocusHere();
                        }
                        ImGui::InputTextWithHint(
                            ("##HiddenPluginSearch" + std::string(idSuffix)).c_str(),
                            localize("General", "sSearch", "Search..."),
                            editorState.hiddenPluginSearch,
                            sizeof(editorState.hiddenPluginSearch));
                        ImGui::Separator();

                        int displayed = 0;
                        for (const auto& plugin : availablePlugins) {
                            if (state.hiddenPlugins.contains(plugin)) continue;
                            if (editorState.hiddenPluginSearch[0] != '\0' && !SharedUtils::ContainsCaseInsensitive(plugin, editorState.hiddenPluginSearch)) continue;

                            if (ImGui::Selectable(plugin.c_str(), false)) {
                                state.hiddenPlugins.insert(plugin);
                                changed = true;
                            }
                            ++displayed;
                            if (displayed >= 300) break;
                        }

                        if (displayed == 0) {
                            ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterNoResults", "No results"));
                        }
                        ImGui::EndCombo();
                    }

                    if (!state.hiddenPlugins.empty()) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton((std::string(localize("General", "sUnhideAll", "Unhide All")) + "##UnhideAll" + std::string(idSuffix)).c_str())) {
                            state.hiddenPlugins.clear();
                            changed = true;
                        }

                        std::vector<std::string> sorted(state.hiddenPlugins.begin(), state.hiddenPlugins.end());
                        std::sort(sorted.begin(), sorted.end(), [](const std::string& a, const std::string& b) {
                            return _stricmp(a.c_str(), b.c_str()) < 0;
                        });
                        std::string toRemove{};
                        for (const auto& plugin : sorted) {
                            ImGui::BulletText("%s", plugin.c_str());
                            ImGui::SameLine();
                            if (ImGui::SmallButton((std::string(localize("General", "sUnhide", "Unhide")) + "##Unhide" + plugin).c_str())) {
                                toRemove = plugin;
                            }
                        }
                        if (!toRemove.empty()) {
                            state.hiddenPlugins.erase(toRemove);
                            changed = true;
                        }
                    }
                }

                ImGui::Spacing();

                if (ImGui::BeginTable(("AdvancedRulesTable" + std::string(idSuffix)).c_str(), 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn(localize("General", "sEnabled", "Enabled"), ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    ImGui::TableSetupColumn(localize("General", "sAdvancedFilterTarget", "Target"), ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn(localize("General", "sAdvancedFilterMatchLabel", "Match"), ImGuiTableColumnFlags_WidthFixed, 120.0f);
                    ImGui::TableSetupColumn(localize("General", "sValue", "Value"), ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn(localize("General", "sAdvancedFilterScope", "Scope"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn(localize("General", "sActions", "Actions"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
                    ImGui::TableHeadersRow();

                    std::size_t removeIndex = state.advancedRules.size();
                    for (std::size_t index = 0; index < state.advancedRules.size(); ++index) {
                        auto& rule = state.advancedRules[index];
                        ImGui::PushID(static_cast<int>(index));
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        changed = ImGui::Checkbox("##Enabled", &rule.enabled) || changed;

                        ImGui::TableSetColumnIndex(1);
                        changed = DrawRuleFieldCombo(localize, "##Field", rule.field) || changed;

                        ImGui::TableSetColumnIndex(2);
                        changed = DrawRuleMatchCombo(localize, "##Match", rule.match) || changed;

                        ImGui::TableSetColumnIndex(3);
                        std::vector<char> buffer((std::max)(rule.value.size() + 1, static_cast<std::size_t>(256)), '\0');
                        std::copy(rule.value.begin(), rule.value.end(), buffer.begin());
                        if (ImGui::InputText("##Value", buffer.data(), buffer.size())) {
                            rule.value = buffer.data();
                            changed = true;
                        }
                        if (rule.match == AdvancedFilterMatch::Regex && !rule.value.empty() && !AdvancedRecordFilters::IsRegexValid(rule.value)) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterInvalidRegex", "Invalid regex"));
                        }

                        ImGui::TableSetColumnIndex(4);
                        changed = DrawRuleScopeCombo(localize, "##Scope", rule.targetPlugins, editorState.ruleScopeSearch, sizeof(editorState.ruleScopeSearch)) || changed;

                        ImGui::TableSetColumnIndex(5);
                        if (ImGui::SmallButton(localize("General", "sRemove", "Remove"))) {
                            removeIndex = index;
                        }

                        ImGui::PopID();
                    }

                    if (removeIndex < state.advancedRules.size()) {
                        state.advancedRules.erase(state.advancedRules.begin() + static_cast<std::ptrdiff_t>(removeIndex));
                        changed = true;
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndChild();

            ImGui::End();
            return changed;
        }
    }

    bool RecordFiltersWidget::Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state)
    {
        bool changed = false;
        auto& editorState = GetEditorState(idSuffix);

        const std::string nonPlayableLabel = std::string(localize("General", "sIncludeNonPlayable", "Include Non-Playable")) + "##NonPlayable" + std::string(idSuffix);
        if (ImGui::Checkbox(nonPlayableLabel.c_str(), &state.showNonPlayable)) {
            changed = true;
        }

        ImGuiWidgetUtils::DrawWrappedSameLine(localize("General", "sIncludeUnnamed", "Include Unnamed"));
        const std::string unnamedLabel = std::string(localize("General", "sIncludeUnnamed", "Include Unnamed")) + "##Unnamed" + std::string(idSuffix);
        if (ImGui::Checkbox(unnamedLabel.c_str(), &state.showUnnamed)) {
            changed = true;
        }

        ImGuiWidgetUtils::DrawWrappedSameLine(localize("General", "sIncludeDeleted", "Include Deleted"));
        const std::string deletedLabel = std::string(localize("General", "sIncludeDeleted", "Include Deleted")) + "##Deleted" + std::string(idSuffix);
        if (ImGui::Checkbox(deletedLabel.c_str(), &state.showDeleted)) {
            changed = true;
        }

        char advancedButtonVisible[160]{};
        const auto activeRules = AdvancedRecordFilters::CountActiveRules(state.advancedRules);
        const auto hiddenCount = state.hiddenPlugins.size();
        if (hiddenCount > 0) {
            std::snprintf(
                advancedButtonVisible,
                sizeof(advancedButtonVisible),
                "%s (%zu+%zu)",
                localize("General", "sAdvancedRecordFilters", "Advanced Filters"),
                activeRules,
                hiddenCount);
        } else {
            std::snprintf(
                advancedButtonVisible,
                sizeof(advancedButtonVisible),
                "%s (%zu)",
                localize("General", "sAdvancedRecordFilters", "Advanced Filters"),
                activeRules);
        }
        char advancedButtonLabel[160]{};
        std::snprintf(
            advancedButtonLabel,
            sizeof(advancedButtonLabel),
            "%s##AdvancedFilters%s",
            advancedButtonVisible,
            std::string(idSuffix).c_str());
        ImGuiWidgetUtils::DrawWrappedSameLine(advancedButtonVisible);
        if (ImGui::GetCursorPosX() > 0.0f) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x * 1.5f);
        }
        if (ImGui::Button(advancedButtonLabel)) {
            editorState.open = !editorState.open;
            if (editorState.open)
                editorState.focusPending = true;
        }

        if (editorState.open) {
            changed = DrawAdvancedFiltersWindow(localize, idSuffix, state, editorState) || changed;
        }

        return changed;
    }

    void RecordFiltersWidget::HandleMenuVisibilityChanged(bool visible)
    {
        auto& states = GetEditorStates();
        for (auto& [_, editorState] : states) {
            if (!visible) {
                editorState.reopenAfterMenuShow = editorState.open;
                editorState.open = false;
                continue;
            }

            editorState.open = editorState.reopenAfterMenuShow;
            editorState.focusPending = editorState.reopenAfterMenuShow;
            editorState.reopenAfterMenuShow = false;
        }
    }
}
