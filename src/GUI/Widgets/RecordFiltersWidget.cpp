#include "GUI/Widgets/RecordFiltersWidget.h"

#include "Filters/AdvancedRecordFilters.h"
#include "Core/CatalogQuery.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/SharedUtils.h"
#include "GUI/Widgets/ModalUtils.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <ranges>
#include <numeric>

namespace ESPExplorerAE
{
    namespace
    {
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
                       (rule.match == AdvancedFilterMatch::Regex ? existingRule.value == rule.value : TextEquals(existingRule.value, rule.value)) &&
                       existingRule.targetPlugins.size() == rule.targetPlugins.size() &&
                       std::ranges::all_of(rule.targetPlugins, [&](const auto& plugin) {
                           return std::ranges::any_of(existingRule.targetPlugins, [&](const auto& existing) { return TextEquals(plugin, existing); });
                       });
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

        void PickerSearch(const char* id, const char* hint, char* buffer, std::size_t size)
        {
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::IsWindowAppearing()) {
                buffer[0] = '\0';
                ImGui::SetKeyboardFocusHere();
            }
            ImGui::InputTextWithHint(id, hint, buffer, size);
        }

        bool DrawRuleScopeCombo(const RecordFiltersWidget::LocalizeFn& localize, const char* id,
            std::vector<std::string>& targetPlugins, AdvancedFilterEditorState& editor)
        {
            bool changed = false;
            const auto label = FormatScopeLabel(localize, targetPlugins);
            if (ImGui::BeginCombo(id, label.c_str(), ImGuiComboFlags_HeightLarge)) {
                PickerSearch("##ScopeSearch", localize("General", "sSearch", "Search..."), editor.ruleScopeSearch, sizeof(editor.ruleScopeSearch));
                if (ImGui::Selectable(localize("General", "sAdvancedFilterScopeClear", "-- All Plugins --"), targetPlugins.empty(), ImGuiSelectableFlags_NoAutoClosePopups)) {
                    changed = !targetPlugins.empty();
                    targetPlugins.clear();
                }
                ImGui::TextWrapped("%s", localize("General", "sAdvancedFilterScopeHint", "Choose plugins, or leave empty for all."));
                std::vector<std::string> choices = targetPlugins;
                for (const auto index : editor.pluginOrder) {
                    const auto& plugin = editor.catalog->plugins[index].filename;
                    if (!std::ranges::any_of(choices, [&](const auto& name) { return TextEquals(name, plugin); })) choices.push_back(plugin);
                }
                std::ranges::sort(choices, [](const auto& a, const auto& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
                std::erase_if(choices, [&](const auto& plugin) { return !TextContains(plugin, editor.ruleScopeSearch); });
                if (ImGui::BeginChild("##ScopeChoices", { 0, ImGui::GetTextLineHeightWithSpacing() * 9 }, ImGuiChildFlags_NavFlattened)) {
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(choices.size()));
                    while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        const auto& plugin = choices[i];
                        const bool selected = std::ranges::any_of(targetPlugins, [&](const auto& name) { return TextEquals(name, plugin); });
                        if (ImGui::Selectable(plugin.c_str(), selected, ImGuiSelectableFlags_NoAutoClosePopups)) {
                            if (selected) std::erase_if(targetPlugins, [&](const auto& name) { return TextEquals(name, plugin); });
                            else targetPlugins.push_back(plugin);
                            changed = true;
                        }
                    }
                    if (choices.empty()) ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterNoResults", "No results"));
                }
                ImGui::EndChild();
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", label.c_str());
            return changed;
        }

        bool DrawKeywordPicker(const RecordFiltersWidget::LocalizeFn& localize, std::string_view suffix,
            RecordFilterState state, AdvancedFilterEditorState& editor)
        {
            bool changed = false;
            const auto label = "##KeywordPicker" + std::string(suffix);
            if (ImGui::BeginCombo(label.c_str(), localize("General", "sAdvancedFilterKeywordPicker", "Add Keyword Rule"), ImGuiComboFlags_HeightLarge)) {
                PickerSearch("##KeywordSearch", localize("General", "sSearch", "Search..."), editor.keywordSearch, sizeof(editor.keywordSearch));
                std::vector<const std::string*> choices;
                for (const auto& keyword : editor.catalog->availableKeywords)
                    if (TextContains(keyword, editor.keywordSearch)) choices.push_back(&keyword);
                if (ImGui::BeginChild("##KeywordChoices", { 0, ImGui::GetTextLineHeightWithSpacing() * 10 }, ImGuiChildFlags_NavFlattened)) {
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(choices.size()));
                    while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                        if (ImGui::Selectable(choices[i]->c_str())) {
                            changed = AddRuleIfMissing(state.advancedRules, { true, AdvancedFilterField::Keyword,
                                AdvancedFilterMatch::Exact, *choices[i], editor.newTargetPlugins }) || changed;
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    if (choices.empty()) ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterNoKeywordResults", "No matching keywords"));
                }
                ImGui::EndChild();
                ImGui::EndCombo();
            }
            return changed;
        }

        bool DrawHiddenPlugins(const RecordFiltersWidget::LocalizeFn& localize, RecordFilterState state, AdvancedFilterEditorState& editor)
        {
            bool changed = false;
            ImGui::TextWrapped("%s", localize("General", "sHiddenPluginsHint", "Check plugins to hide their records."));
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##HiddenPluginSearch", localize("General", "sSearch", "Search..."), editor.hiddenPluginSearch, sizeof(editor.hiddenPluginSearch));
            ImGui::BeginDisabled(state.hiddenPlugins.empty());
            if (ImGui::Button(localize("General", "sUnhideAll", "Unhide All"))) {
                state.hiddenPlugins.clear();
                changed = true;
            }
            ImGui::EndDisabled();
            std::vector<std::string> choices(state.hiddenPlugins.begin(), state.hiddenPlugins.end());
            for (const auto index : editor.pluginOrder) {
                const auto& plugin = editor.catalog->plugins[index].filename;
                if (std::ranges::find(choices, plugin) == choices.end()) choices.push_back(plugin);
            }
            std::ranges::sort(choices, [](const auto& a, const auto& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
            std::erase_if(choices, [&](const auto& plugin) { return !TextContains(plugin, editor.hiddenPluginSearch); });
            if (ImGui::BeginChild("##HiddenPluginsList", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened)) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(choices.size()), ImGui::GetFrameHeightWithSpacing());
                while (clipper.Step()) for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& plugin = choices[i];
                    ImGui::PushID(plugin.c_str());
                    bool hidden = state.hiddenPlugins.contains(plugin);
                    if (ImGui::Checkbox("##Hidden", &hidden)) {
                        if (hidden) state.hiddenPlugins.insert(plugin);
                        else state.hiddenPlugins.erase(plugin);
                        changed = true;
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted(plugin.c_str());
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", plugin.c_str());
                    ImGui::PopID();
                }
                if (choices.empty()) ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterNoResults", "No results"));
            }
            ImGui::EndChild();
            return changed;
        }

        bool DrawAdvancedFiltersWindow(const RecordFiltersWidget::LocalizeFn& localize, std::string_view idSuffix,
            RecordFilterState state, AdvancedFilterEditorState& editor)
        {
            const auto title = std::string(localize("General", "sAdvancedRecordFilters", "Advanced Filters")) + " - " +
                ScopeLabel(localize, idSuffix) + "###AdvancedFiltersWindow" + std::string(idSuffix);
            const float scale = ImGui::GetFontSize() / 20.0f;
            ModalUtils::PrepareToolWindow(title.c_str(), { 960 * scale, 740 * scale }, { 540 * scale, 460 * scale }, editor.focusPending);
            if (!ImGui::Begin(title.c_str(), &editor.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing)) {
                ImGui::End();
                return false;
            }
            if (ModalUtils::EscapeClosesCurrentWindow()) editor.open = false;
            if (!editor.open) { ImGui::End(); return false; }

            const AdvancedFilterEditorState::UndoState before{ state.advancedRules, state.hiddenPlugins };
            if (editor.undo && (!editor.undoResult || editor.undoResult->rules != state.advancedRules || editor.undoResult->hiddenPlugins != state.hiddenPlugins)) {
                editor.undo.reset();
                editor.undoResult.reset();
            }
            bool changed = false;
            bool undoing = false;
            ImGui::TextWrapped("%s", localize("General", "sAdvancedFilterExplanation", "Matching rules hide records across browsers. Text ignores case; changes save automatically."));
            ImGui::TextDisabled("%zu %s | %zu %s", AdvancedRecordFilters::CountActiveRules(state.advancedRules),
                localize("General", "sAdvancedFiltersActiveSummary", "active block rules"), state.hiddenPlugins.size(),
                localize("General", "sAdvancedFiltersHiddenPluginsSummary", "hidden plugins"));
            ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(localize("General", "sUndoFilterChange", "Undo Last Change")).x + ImGui::GetStyle().FramePadding.x * 2);
            ImGui::BeginDisabled(!editor.undo);
            if (ImGui::Button(localize("General", "sUndoFilterChange", "Undo Last Change")) && editor.undo) {
                state.advancedRules = std::move(editor.undo->rules);
                state.hiddenPlugins = std::move(editor.undo->hiddenPlugins);
                editor.undo.reset();
                changed = undoing = true;
            }
            ImGui::EndDisabled();

            if (ImGui::BeginTabBar("##FilterSections")) {
                const auto rulesTitle = std::string(localize("General", "sBlockRules", "Block Rules")) + " (" + std::to_string(state.advancedRules.size()) + ")###Rules";
                if (ImGui::BeginTabItem(rulesTitle.c_str())) {
                    const auto composerTitle = std::string(localize("General", "sAdvancedFilterAddRule", "Add Rule")) + "###NewRuleComposer";
                    ImGui::SetNextItemOpen(state.advancedRules.empty(), ImGuiCond_Once);
                    if (ImGui::CollapsingHeader(composerTitle.c_str())) {
                        // The composer stays compact; the rule list owns its scrolling.
                        auto field = static_cast<AdvancedFilterField>(editor.newField);
                        auto match = static_cast<AdvancedFilterMatch>(editor.newMatch);
                        if (ImGui::BeginTable("##NewRuleFields", 2, ImGuiTableFlags_SizingStretchSame)) {
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterTarget", "Target"));
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            if (DrawRuleFieldCombo(localize, "##AdvancedField", field)) editor.newField = static_cast<int>(field);
                            ImGui::TableNextColumn();
                            ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterMatchLabel", "Match"));
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            if (DrawRuleMatchCombo(localize, "##AdvancedMatch", match)) editor.newMatch = static_cast<int>(match);
                            ImGui::EndTable();
                        }
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        const bool enter = ImGui::InputTextWithHint("##AdvancedValue", localize("General", "sAdvancedFilterValueHint", "Value or pattern"),
                            editor.newValue, sizeof(editor.newValue), ImGuiInputTextFlags_EnterReturnsTrue);
                        const bool invalid = match == AdvancedFilterMatch::Regex && editor.newValue[0] && !AdvancedRecordFilters::IsRegexValid(editor.newValue);
                        if (invalid) ImGui::TextWrapped("%s", localize("General", "sAdvancedFilterInvalidDraft", "Invalid regex. Correct the pattern before adding this rule."));
                        ImGui::TextDisabled("%s", localize("General", "sAdvancedFilterScope", "Scope"));
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        DrawRuleScopeCombo(localize, "##NewRuleScope", editor.newTargetPlugins, editor);
                        bool first = true;
                        ImGui::BeginDisabled(!editor.newValue[0] || invalid);
                        const auto addLabel = std::string(localize("General", "sAdvancedFilterAddRule", "Add Rule")) + "##AddRule" + std::string(idSuffix);
                        const bool add = ImGuiWidgetUtils::DrawWrappedButton(addLabel.c_str(), first);
                        const AdvancedFilterRule draft{ true, field, match, editor.newValue, editor.newTargetPlugins };
                        if (editor.previewRule && *editor.previewRule != draft) editor.previewRule.reset();
                        if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sPreviewRule", "Preview Matches"), first)) {
                            editor.previewRule = draft;
                            editor.previewCount = 0;
                            editor.previewSamples.clear();
                            const PreparedRecordFilters preview(std::span(&draft, 1));
                            for (RecordIndex index = 0; index < editor.catalog->records.size(); ++index) {
                                if (preview.Passes(editor.catalog->records[index])) continue;
                                ++editor.previewCount;
                                if (editor.previewSamples.size() < 5) editor.previewSamples.push_back(index);
                            }
                        }
                        ImGui::EndDisabled();
                        if ((add || enter) && editor.newValue[0] && !invalid) {
                            changed = AddRuleIfMissing(state.advancedRules, { true, field, match, editor.newValue, editor.newTargetPlugins }) || changed;
                            editor.newValue[0] = '\0';
                            editor.previewRule.reset();
                        }
                        ImGuiWidgetUtils::SameLineIfFits(ImGui::GetFontSize() * 17);
                        ImGui::SetNextItemWidth((std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 17));
                        changed = DrawKeywordPicker(localize, idSuffix, state, editor) || changed;
                        if (editor.previewRule) {
                            ImGui::Text("%zu %s", editor.previewCount, localize("General", "sPreviewRuleCount", "matching runtime records"));
                            if (ImGui::TreeNode("##PreviewSamples", "%s", localize("General", "sPreviewRuleSamples", "Sample matches"))) {
                                ImGui::TextWrapped("%s", localize("General", "sPreviewRuleHint", "Up to five matches before other filters apply."));
                                for (const auto index : editor.previewSamples) {
                                    const auto& record = editor.catalog->records[index];
                                    ImGui::TextWrapped("%08X | %s | %s", record.formID, record.name.empty() ? record.editorID.c_str() : record.name.c_str(), record.sourcePlugin.c_str());
                                }
                                ImGui::TreePop();
                            }
                        }
                    }
                    ImGui::Separator();
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ImGui::InputTextWithHint("##RuleSearch", localize("General", "sSearchRules", "Search rules, fields, or plugin scopes..."), editor.ruleSearch, sizeof(editor.ruleSearch));
                    bool first = true;
                    ImGui::BeginDisabled(state.advancedRules.empty());
                    if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sEnableAllRules", "Enable All"), first)) {
                        for (auto& rule : state.advancedRules) { changed = !rule.enabled || changed; rule.enabled = true; }
                    }
                    if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sDisableAllRules", "Disable All"), first)) {
                        for (auto& rule : state.advancedRules) { changed = rule.enabled || changed; rule.enabled = false; }
                    }
                    const auto clearLabel = std::string(localize("General", "sClearAll", "Clear All")) + "##ClearAdvanced" + std::string(idSuffix);
                    if (ImGuiWidgetUtils::DrawWrappedButton(clearLabel.c_str(), first)) { state.advancedRules.clear(); changed = true; }
                    ImGui::EndDisabled();
                    const auto defaultsLabel = std::string(localize("General", "sAdvancedFilterRestoreDefaults", "Restore Defaults")) + "##RestoreDefaults" + std::string(idSuffix);
                    if (ImGuiWidgetUtils::DrawWrappedButton(defaultsLabel.c_str(), first))
                        for (const auto& rule : AdvancedRecordFilters::GetDefaultRules()) changed = AddRuleIfMissing(state.advancedRules, rule) || changed;

                    if (ImGui::BeginChild("##RuleList", { 0, (std::max)(ImGui::GetTextLineHeightWithSpacing() * 3, ImGui::GetContentRegionAvail().y) }, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened)) {
                        std::optional<std::size_t> remove;
                        std::size_t displayed{};
                        for (std::size_t index = 0; index < state.advancedRules.size(); ++index) {
                            auto& rule = state.advancedRules[index];
                            if (!TextContains(rule.value, editor.ruleSearch) && !TextContains(FieldLabel(localize, rule.field), editor.ruleSearch) &&
                                !TextContains(MatchLabel(localize, rule.match), editor.ruleSearch) && !TextContains(FormatScopeLabel(localize, rule.targetPlugins), editor.ruleSearch) &&
                                !std::ranges::any_of(rule.targetPlugins, [&](const auto& plugin) { return TextContains(plugin, editor.ruleSearch); })) continue;
                            ++displayed;
                            ImGui::PushID(static_cast<int>(index));
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                            if (ImGui::BeginChild("##Rule", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_NavFlattened)) {
                                changed = ImGui::Checkbox(localize("General", "sEnabled", "Enabled"), &rule.enabled) || changed;
                                ImGui::SameLine();
                                ImGui::TextDisabled("#%zu", index + 1);
                                ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(localize("General", "sRemove", "Remove")).x + ImGui::GetStyle().FramePadding.x * 2);
                                if (ImGui::SmallButton(localize("General", "sRemove", "Remove"))) remove = index;
                                if (ImGui::BeginTable("##RuleFields", 2, ImGuiTableFlags_SizingStretchSame)) {
                                    ImGui::TableNextColumn();
                                    ImGui::SetNextItemWidth(-FLT_MIN);
                                    changed = DrawRuleFieldCombo(localize, "##Field", rule.field) || changed;
                                    ImGui::TableNextColumn();
                                    ImGui::SetNextItemWidth(-FLT_MIN);
                                    changed = DrawRuleMatchCombo(localize, "##Match", rule.match) || changed;
                                    ImGui::EndTable();
                                }
                                std::vector<char> buffer(rule.value.size() + 1024, '\0');
                                std::copy(rule.value.begin(), rule.value.end(), buffer.begin());
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                if (ImGui::InputText("##Value", buffer.data(), buffer.size())) { rule.value = buffer.data(); changed = true; }
                                if (rule.value.empty()) ImGui::TextWrapped("%s", localize("General", "sAdvancedFilterEmptyRule", "Enter a value to activate this rule."));
                                else if (rule.match == AdvancedFilterMatch::Regex && !AdvancedRecordFilters::IsRegexValid(rule.value))
                                    ImGui::TextWrapped("%s", localize("General", "sAdvancedFilterInvalidRule", "Invalid regex. This rule is ignored until the pattern is corrected."));
                                ImGui::SetNextItemWidth(-FLT_MIN);
                                changed = DrawRuleScopeCombo(localize, "##Scope", rule.targetPlugins, editor) || changed;
                            }
                            ImGui::EndChild();
                            ImGui::PopStyleColor();
                            ImGui::PopID();
                        }
                        if (remove) { state.advancedRules.erase(state.advancedRules.begin() + static_cast<std::ptrdiff_t>(*remove)); changed = true; }
                        if (!displayed) ImGui::TextWrapped("%s", state.advancedRules.empty() ?
                            localize("General", "sNoBlockRules", "No block rules. Add a rule above or restore the defaults.") :
                            localize("General", "sNoMatchingRules", "No matching rules. Clear the search to see all rules."));
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                const auto hiddenTitle = std::string(localize("General", "sHiddenPlugins", "Hidden Plugins")) + " (" + std::to_string(state.hiddenPlugins.size()) + ")###HiddenPlugins";
                if (ImGui::BeginTabItem(hiddenTitle.c_str())) {
                    changed = DrawHiddenPlugins(localize, state, editor) || changed;
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            if (changed && !undoing) {
                editor.undo = before;
                editor.undoResult = { state.advancedRules, state.hiddenPlugins };
            }
            ImGui::End();
            return changed;
        }
    }

    void RecordFiltersWidget::DrawWhyHidden(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state,
        AdvancedFilterEditorState& editor, const CatalogQuery& query, std::shared_ptr<const CatalogSnapshot> catalog)
    {
        ImGuiWidgetUtils::DrawWrappedSameLine(localize("Search", "sWhyHidden", "Why Hidden?"));
        ImGui::PushID(idSuffix.data());
        if (ImGui::Button(localize("Search", "sWhyHidden", "Why Hidden?"))) ImGui::OpenPopup("WhyHidden");
        if (ImGui::BeginPopup("WhyHidden")) {
            ImGui::SetNextItemWidth(220);
            ImGui::InputTextWithHint("##HiddenID", localize("Search", "sExactID", "Eight-digit FormID"), editor.hiddenFormID, sizeof(editor.hiddenFormID), ImGuiInputTextFlags_CharsHexadecimal);
            const auto id = ParseExactFormID(editor.hiddenFormID);
            const auto* record = id && catalog ? catalog->Find(*id) : nullptr;
            if (record) {
                ImGui::TextWrapped("%s", record->name.c_str());
                auto local = query;
                local.hiddenPlugins.clear(); local.showPlayable = true; local.showNonPlayable = true;
                local.showNamed = true; local.showUnnamed = true; local.showDeleted = true;
                if (!local.Matches(*record, PreparedRecordFilters{})) ImGui::TextWrapped("%s", localize("Search", "sOutsideQuery", "Excluded by this view's query, category, or scope."));
                bool blocked = false;
                if (state.hiddenPlugins.contains(record->sourcePlugin)) { blocked = true; ImGui::TextWrapped("%s", localize("Search", "sPluginHidden", "The source plugin is hidden by a shared rule.")); }
                if ((!record->isPlayable && !state.showNonPlayable) || (record->name.empty() && !state.showUnnamed) || (record->isDeleted && !state.showDeleted)) {
                    blocked = true; ImGui::TextWrapped("%s", localize("Search", "sVisibilityHidden", "Excluded by the shared playable/name/deleted visibility controls."));
                }
                for (std::size_t index = 0; index < state.advancedRules.size(); ++index) {
                    const auto& rule = state.advancedRules[index];
                    if (PreparedRecordFilters(std::span(&rule, 1)).Passes(*record)) continue;
                    blocked = true;
                    ImGui::PushID(static_cast<int>(index));
                    ImGui::TextWrapped("#%zu: %s", index + 1, rule.value.c_str());
                    if (ImGui::Button(localize("Search", "sReviewRule", "Review rule and preview"))) {
                        editor.UpdateChoices(catalog);
                        editor.newField = static_cast<int>(rule.field); editor.newMatch = static_cast<int>(rule.match);
                        std::snprintf(editor.newValue, sizeof(editor.newValue), "%s", rule.value.c_str());
                        editor.newTargetPlugins = rule.targetPlugins;
                        editor.open = true; editor.focusPending = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                }
                if (!blocked) ImGui::TextWrapped("%s", localize("Search", "sNoGlobalExclusion", "No shared visibility rule excludes this record."));
            } else ImGui::TextWrapped("%s", localize("Search", "sRecordNotFound", "Enter a valid ID present in the current runtime catalog."));
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    bool RecordFiltersWidget::Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state,
        AdvancedFilterEditorState& editorState, std::shared_ptr<const CatalogSnapshot> catalog)
    {
        bool changed = false;

        const auto activeRules = AdvancedRecordFilters::CountActiveRules(state.advancedRules);
        const auto hiddenCount = state.hiddenPlugins.size();
        const auto visibility = std::string(localize("General", "sGlobalVisibilityRules", "Global Visibility Rules")) + " (" +
            std::to_string(activeRules) + "+" + std::to_string(hiddenCount) + ")###Visibility" + std::string(idSuffix);
        const auto popup = "VisibilityOptions" + std::string(idSuffix);
        if (ImGui::Button(visibility.c_str())) ImGui::OpenPopup(popup.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", localize("General", "sGlobalVisibilityHint", "Applies across Explore. Clearing search or resetting a view keeps these rules."));
        if (ImGui::BeginPopup(popup.c_str())) {
            const auto nonPlayable = std::string(localize("General", "sIncludeNonPlayable", "Include Non-Playable")) + "##NonPlayable" + std::string(idSuffix);
            const auto unnamed = std::string(localize("General", "sIncludeUnnamed", "Include Unnamed")) + "##Unnamed" + std::string(idSuffix);
            const auto deleted = std::string(localize("General", "sIncludeDeleted", "Include Deleted")) + "##Deleted" + std::string(idSuffix);
            changed = ImGui::Checkbox(nonPlayable.c_str(), &state.showNonPlayable) || changed;
            changed = ImGui::Checkbox(unnamed.c_str(), &state.showUnnamed) || changed;
            changed = ImGui::Checkbox(deleted.c_str(), &state.showDeleted) || changed;
            ImGui::EndPopup();
        }
        const auto visibleLabel = std::string(localize("General", "sAdvancedRecordFilters", "Advanced Filters"));
        const auto advancedButtonLabel = visibleLabel + "###AdvancedFilters" + std::string(idSuffix);
        ImGuiWidgetUtils::DrawWrappedSameLine(visibleLabel.c_str());
        if (ImGui::Button(advancedButtonLabel.c_str())) { editorState.open = true; editorState.focusPending = true; }

        // Editors are submitted by the window owner after the active browser,
        // so changing tabs does not hide an open tool window.
        (void)catalog;
        return changed;
    }

    bool RecordFiltersWidget::DrawEditor(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state,
        AdvancedFilterEditorState& editorState, std::shared_ptr<const CatalogSnapshot> catalog)
    {
        bool changed = false;
        if (editorState.open && catalog) {
            editorState.UpdateChoices(std::move(catalog));
            changed = DrawAdvancedFiltersWindow(localize, idSuffix, state, editorState);
        }
        if (!editorState.open) editorState.UpdateChoices({});
        return changed;
    }

    void AdvancedFilterEditorState::UpdateChoices(std::shared_ptr<const CatalogSnapshot> snapshot)
    {
        if (catalog == snapshot) return;
        catalog = std::move(snapshot);
        previewRule.reset();
        previewSamples.clear();
        pluginOrder.clear();
        if (!catalog) return;
        pluginOrder.resize(catalog->plugins.size());
        std::iota(pluginOrder.begin(), pluginOrder.end(), std::size_t{});
        std::ranges::stable_sort(pluginOrder, [&](auto left, auto right) {
            const auto& a = catalog->plugins[left].filename;
            const auto& b = catalog->plugins[right].filename;
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                [](unsigned char x, unsigned char y) { return FoldASCII(x) < FoldASCII(y); });
        });
    }

    void AdvancedFilterEditorState::HandleMenuVisibilityChanged(bool visible)
    {
        if (menuVisible == visible) return;
        menuVisible = visible;
        if (!visible) {
            reopenAfterMenuShow = open;
            open = false;
            // Choices will be reacquired from the supplied catalog on reopening.
            catalog.reset();
            pluginOrder.clear();
            undo.reset();
            undoResult.reset();
        } else {
            open = reopenAfterMenuShow;
            focusPending = reopenAfterMenuShow;
            reopenAfterMenuShow = false;
        }
    }
}
