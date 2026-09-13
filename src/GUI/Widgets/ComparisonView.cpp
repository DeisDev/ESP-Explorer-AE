#include "GUI/Widgets/ComparisonView.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "Core/Workspace.h"
#include <imgui.h>

namespace ESPExplorerAE
{
    void DrawComparisonWindow(ComparisonViewState& state, const BrowserView& view, BrowserRequests& requests)
    {
        if (!state.open) return;
        const auto& localize = view.localize;
        const auto title = std::string(localize("Comparison", "sCompare", "Compare")) + "###WorkspaceComparison";
        ModalUtils::PrepareToolWindow(title.c_str(), {900, 650}, {440, 300}, state.focusPending);
        if (ImGui::Begin(title.c_str(), &state.open, ImGuiWindowFlags_NoCollapse)) {
            if (ModalUtils::EscapeClosesCurrentWindow()) state.open = false;
            ImGui::TextWrapped("%s", localize("Comparison", "sCapturedHint", "Damage excludes player bonuses. Values stay fixed until you compare again."));
            if (!state.a || !state.b) ImGui::TextWrapped("%s", localize("Comparison", "sChooseTargets", "Choose Pin A on one record, then Compare B on another."));
            if (state.a && state.b) {
                const auto& a = *state.a; const auto& b = *state.b;
                ImGuiWidgetUtils::DrawStatusArea("##ComparisonFeedback", [&] {
                    if (a.session != view.session || b.session != view.session) ImGui::TextWrapped("%s", localize("Comparison", "sExpired", "Previous game session. Select both targets again to follow links."));
                });
                ImGui::Checkbox(localize("Comparison", "sDifferencesOnly", "Differences Only"), &state.differencesOnly);
                const auto label = [&](ComparisonField field) {
                    switch (field) {
                    case ComparisonField::Name: return localize("General", "sName", "Name");
                    case ComparisonField::Type: return localize("General", "sType", "Type");
                    case ComparisonField::Identity: return localize("Comparison", "sIdentity", "Identity");
                    case ComparisonField::Damage: return localize("Comparison", "sCapturedDamage", "Captured Damage");
                    case ComparisonField::Armor: return localize("Comparison", "sArmorRating", "Armor Rating");
                    case ComparisonField::Weight: return localize("General", "sWeight", "Weight");
                    case ComparisonField::Value: return localize("General", "sValue", "Value");
                    case ComparisonField::Ammo: return localize("Items", "sAmmo", "Ammunition");
                    case ComparisonField::Keywords: return localize("Comparison", "sCapturedKeywords", "Captured Keyword Set");
                    case ComparisonField::Attachments: return localize("Inventory", "sCurrentMods", "Current Mods");
                    case ComparisonField::Enchantments: return localize("Inventory", "sEnchantments", "Enchantments");
                    case ComparisonField::Condition: return localize("Inventory", "sCondition", "Condition");
                    case ComparisonField::Quantity: return localize("General", "sQuantity", "Quantity");
                    case ComparisonField::Equipped: return localize("Inventory", "sEquipped", "Equipped");
                    default: return "";
                    }
                };
                const auto text = [&](const ComparisonValue& value) -> std::string {
                    if (const auto* number = std::get_if<double>(&value)) return std::format("{:g}", *number);
                    if (const auto* name = std::get_if<std::string>(&value)) return *name;
                    if (const auto* flag = std::get_if<bool>(&value)) return *flag ? localize("General", "sYes", "Yes") : localize("General", "sNo", "No");
                    if (const auto* references = std::get_if<ComparisonReferences>(&value)) {
                        if (references->empty()) return localize("General", "sNone", "None");
                        std::string result;
                        for (const auto& reference : *references) {
                            if (!result.empty()) result += "\n";
                            const auto* record = view.catalog->Find(reference.formID);
                            result += (record ? record->name + " " : "") + FormatUtils::FormID(reference.formID);
                            if (reference.slot || reference.rank || reference.disabled) result += std::format(" [{}:{}:{}]", reference.slot, reference.rank, reference.disabled ? 1 : 0);
                        }
                        return result;
                    }
                    return localize("General", "sUnavailable", "Unavailable");
                };
                const auto kind = [&](const auto& record) { return record.instance ? localize("Comparison", "sInstance", "Inventory Instance") : localize("Comparison", "sBase", "Base Record"); };
                const auto rows = ComparisonRows(a, b, state.differencesOnly);
                if (ImGui::Button(localize("Comparison", "sExport", "Copy Comparison"))) {
                    std::string output = std::string(kind(a)) + ": " + a.name + "\n" + kind(b) + ": " + b.name + "\n";
                    for (auto field : rows) output += std::string(label(field)) + "\t" + text(a.At(field)) + "\t" + text(b.At(field)) + "\n";
                    ImGui::SetClipboardText(output.c_str());
                }
                if (ImGui::BeginTable("ComparisonValues", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, {0, 0})) {
                    ImGui::TableSetupColumn(localize("Comparison", "sProperty", "Property"));
                    const auto headingA = std::string("A: ") + kind(a) + " - " + a.name; const auto headingB = std::string("B: ") + kind(b) + " - " + b.name;
                    ImGui::TableSetupColumn(headingA.c_str()); ImGui::TableSetupColumn(headingB.c_str()); ImGui::TableSetupScrollFreeze(0, 1); ImGui::TableHeadersRow();
                    for (auto field : rows) {
                        ImGui::PushID(static_cast<int>(field)); ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextWrapped("%s", label(field));
                        int side{};
                        for (const auto* record : {&a, &b}) {
                            ImGui::TableNextColumn(); ImGui::PushID(side++);
                            const auto& value = record->At(field);
                            if (const auto* refs = std::get_if<ComparisonReferences>(&value); refs && !refs->empty()) {
                                for (std::size_t i = 0; i < refs->size(); ++i) {
                                    const auto& ref = (*refs)[i]; ImGui::PushID(static_cast<int>(i));
                                    const bool enabled = record->session == view.session && view.catalog->Find(ref.formID);
                                    ImGui::BeginDisabled(!enabled);
                                    if (ImGui::Selectable(text(ComparisonReferences{ref}).c_str())) requests.inspections.push_back(ref.formID);
                                    ImGui::EndDisabled(); ImGui::PopID();
                                }
                            } else ImGui::TextWrapped("%s", text(value).c_str());
                            ImGui::PopID();
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }
}
