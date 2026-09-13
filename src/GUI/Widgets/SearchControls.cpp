#include "GUI/Widgets/SearchControls.h"

#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include <imgui.h>
#include <cstdio>

namespace ESPExplorerAE
{
    void DrawSearchControls(bool& structured, RecordScope& scope, std::string& search, char* buffer, std::size_t size,
        const CatalogSnapshot& catalog, const std::function<const char*(std::string_view, std::string_view, const char*)>& localize)
    {
        const auto L = [&](const char* key, const char* fallback) { return localize("Search", key, fallback); };
        const std::array scopes{L("sAllPlugins", "All Plugins"), L("sSelectedPlugins", "Selected Plugins"), L("sCollection", "Collection")};
        float scopeWidth = 0.0f;
        for (const auto* label : scopes) scopeWidth = (std::max)(scopeWidth, ImGui::CalcTextSize(label).x);
        ImGui::SetNextItemWidth((std::min)(scopeWidth + ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2, ImGui::GetContentRegionAvail().x));
        if (ImGui::BeginCombo("##SearchScope", scopes[static_cast<std::size_t>(scope.kind)])) {
            for (std::size_t index = 0; index < scopes.size(); ++index) {
                if (ImGui::Selectable(scopes[index], static_cast<std::size_t>(scope.kind) == index)) scope.kind = static_cast<SearchScope>(index);
            }
            ImGui::EndCombo();
        }
        if (scope.kind == SearchScope::SelectedPlugins) {
            const auto label = std::string(L("sSelectedPlugins", "Selected Plugins")) + " (" + std::to_string(scope.plugins.size()) + ")###ScopePlugins";
            ImGuiWidgetUtils::DrawWrappedSameLine(label.c_str());
            if (ImGui::Button(label.c_str())) ImGui::OpenPopup("ScopePluginList");
            if (ImGui::BeginPopup("ScopePluginList")) {
                if (ImGui::BeginChild("ScopePlugins", {320, 260})) {
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(catalog.plugins.size()));
                    while (clipper.Step()) for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                        const auto& plugin = catalog.plugins[row];
                        const auto found = std::ranges::find(scope.plugins, plugin.filename);
                        bool selected = found != scope.plugins.end();
                        if (ImGui::Checkbox(plugin.filename.c_str(), &selected)) {
                            if (selected) scope.plugins.push_back(plugin.filename);
                            else scope.plugins.erase(found);
                        }
                    }
                }
                ImGui::EndChild();
                ImGui::EndPopup();
            }
            if (scope.plugins.empty()) ImGui::TextWrapped("%s", L("sEmptyPlugins", "No plugins selected. This scope has no results."));
        } else if (scope.kind == SearchScope::Collection) {
            ImGui::TextWrapped("%s: %s", scopes[2], scope.collection.empty() ? L("sChooseCollection", "Choose a collection in Sources/Views.") : scope.collection.c_str());
        }
        ImGuiWidgetUtils::DrawWrappedSameLine(L("sStructured", "Structured search"));
        ImGui::Checkbox(L("sStructured", "Structured search"), &structured);
        if (structured) {
            ImGuiWidgetUtils::DrawWrappedSameLine(L("sInsertField", "Insert search field"));
            if (ImGui::Button(L("sInsertField", "Insert search field"))) ImGui::OpenPopup("SearchFields");
            if (ImGui::BeginPopup("SearchFields")) {
                struct Field { const char* key; const char* label; const char* token; };
                constexpr Field fields[]{
                    {"sName", "Name (name:)", "name:"}, {"sType", "Type (type:)", "type:"}, {"sPlugin", "Plugin (plugin:)", "plugin:\"\""},
                    {"sEditorID", "EditorID (editorid:)", "editorid:"}, {"sID", "FormID (id:)", "id:"}, {"sKeyword", "Keyword (keyword:)", "keyword:"},
                    {"sDamage", "Base damage (damage:)", "damage:>"}, {"sWeight", "Weight (weight:)", "weight:<"}, {"sValue", "Value (value:)", "value:>"}
                };
                const auto append = [&](std::string_view token) {
                    const auto next = search + (search.empty() ? "" : " ") + std::string(token);
                    if (next.size() < size) { search = next; std::snprintf(buffer, size, "%s", search.c_str()); }
                };
                for (const auto& field : fields) {
                    const std::string_view key = field.key;
                    if (key == "sPlugin" || key == "sType" || key == "sKeyword") {
                        if (ImGui::BeginMenu(L(field.key, field.label))) {
                            // Suggestions come from already captured indexes. Only the
                            // small type set needs ordering; large lists stay clipped.
                            std::vector<std::string> types;
                            if (key == "sType") {
                                for (const auto& [type, rows] : catalog.byType) if (!rows.empty()) types.push_back(type);
                                std::ranges::sort(types);
                            }
                            const auto count = key == "sPlugin" ? catalog.plugins.size() : key == "sKeyword" ? catalog.availableKeywords.size() : types.size();
                            if (ImGui::BeginChild("FieldValues", {320, 240}, ImGuiChildFlags_NavFlattened)) {
                                ImGuiListClipper clipper;
                                clipper.Begin(static_cast<int>(count));
                                while (clipper.Step()) for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                                    const auto& value = key == "sPlugin" ? catalog.plugins[index].filename : key == "sKeyword" ? catalog.availableKeywords[index] : types[index];
                                    if (ImGui::Selectable(value.c_str())) {
                                        std::string token = key == "sPlugin" ? "plugin:\"" : key == "sType" ? "type:\"" : "keyword:\"";
                                        for (char ch : value) { if (ch == '"' || ch == '\\') token += '\\'; token += ch; }
                                        append(token + '"');
                                        ImGui::CloseCurrentPopup();
                                    }
                                }
                            }
                            ImGui::EndChild(); ImGui::EndMenu();
                        }
                    } else if (ImGui::MenuItem(L(field.key, field.label))) append(field.token);
                }
                ImGui::EndPopup();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
                ImGui::TextUnformatted(L("sExamples", "type:WEAP plugin:\"Example Weapons.esp\" -name:debug\nkeyword:WeaponTypeRifle\neditorid:MyMod_\nid:01234567\nweight:<10"));
                ImGui::PopTextWrapPos(); ImGui::EndTooltip();
            }
            const auto parsed = ParseSearch(search);
            if (!parsed) {
                const char* error{};
                switch (parsed.error) {
                case SearchError::InvalidID: error = L("sInvalidID", "FormID must be exactly eight hexadecimal digits and nonzero."); break;
                case SearchError::InvalidNumber: error = L("sInvalidNumber", "Use a finite number with =, <, >, <=, or >=."); break;
                case SearchError::Quote: error = L("sInvalidQuote", "Close quoted values; only quotes and backslashes can be escaped."); break;
                case SearchError::Field: error = L("sInvalidField", "Unknown search field. Use Insert search field for supported fields."); break;
                case SearchError::EmptyValue: error = L("sEmptyValue", "Each search field needs a value."); break;
                default: error = L("sTooLong", "Search is too long or contains too many clauses."); break;
                }
                ImGui::TextWrapped("%s", error);
            }
        }
    }
}
