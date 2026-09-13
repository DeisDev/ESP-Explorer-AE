#include "GUI/Widgets/ActionHistoryView.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/ModalUtils.h"
#include "Core/CatalogQuery.h"

#include <cfloat>

namespace ESPExplorerAE
{
    ImVec2 DrawActionHistoryWindow(ActionHistoryViewState& state, std::span<const ActionReceipt> receipts,
        const ActionHistoryLocalize& localize, ImVec2 initialPosition)
    {
        if (!state.open) return initialPosition;
        const auto L = [&](auto key, auto fallback) { return localize("General", key, fallback); };
        const auto title = std::string(L("sActionHistory", "Action History")) + "###ActionHistoryWindow";
        const float scale = ImGui::GetFontSize() / 20.0f;
        ModalUtils::PrepareToolWindow(title.c_str(), { 760 * scale, 580 * scale }, { 420 * scale, 320 * scale }, state.focusPending, &initialPosition);
        const bool visible = ImGui::Begin(title.c_str(), &state.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        const auto position = ImGui::GetWindowPos();
        if (ModalUtils::EscapeClosesCurrentWindow()) state.open = false;
        if (visible && state.open) {
            ImGui::TextWrapped("%s", L("sActionHistoryHint", "Newest actions appear first. Dispatched means the request was sent; only verified changes confirm an observed result."));
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##HistorySearch", L("sSearchActionHistory", "Search actions, results, or details..."), state.search.data(), state.search.size());
            ImGui::Checkbox(L("sActionHistoryIssuesOnly", "Needs attention only"), &state.issuesOnly);
            const auto history = FormatActionHistory(receipts, localize);
            std::size_t displayed{};
            std::uint64_t lastGroup{};
            if (ImGui::BeginChild("##ActionHistoryList", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened)) {
                for (const auto& entry : history) {
                    if (state.issuesOnly && entry.result != ActionStatus::Rejected && entry.result != ActionStatus::Failed && entry.result != ActionStatus::NoChange) continue;
                    if (!SearchContains(entry.description, state.search.data()) && !SearchContains(entry.status, state.search.data()) && !SearchContains(entry.details, state.search.data()) && !SearchContains(entry.groupName, state.search.data())) continue;
                    if (entry.groupID && entry.groupID != lastGroup) ImGui::SeparatorText((entry.groupName + " #" + std::to_string(entry.groupID)).c_str());
                    lastGroup = entry.groupID;
                    ++displayed;
                    const auto id = std::to_string(entry.id);
                    ImGui::PushID(id.c_str());
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
                    if (ImGui::BeginChild("##HistoryEntry", { 0, 0 }, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_NavFlattened)) {
                        ImGui::TextDisabled("#%s", id.c_str());
                        ImGui::SameLine();
                        ImGui::TextWrapped("%s", entry.status.c_str());
                        ImGui::TextWrapped("%s", entry.description.c_str());
                        if (ImGui::SmallButton(L("sCopy", "Copy"))) {
                            const auto text = "#" + id + " | " + entry.status + "\n" + entry.description + (entry.details.empty() ? "" : "\n" + entry.details);
                            ImGui::SetClipboardText(text.c_str());
                        }
                        if (entry.target) {
                            ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(L("sInspect", "Inspect")).x + ImGui::GetFrameHeight());
                            if (ImGui::SmallButton(L("sInspect", "Inspect"))) state.inspect = entry.target;
                        }
                        if (!entry.details.empty()) {
                            ImGuiWidgetUtils::SameLineIfFits(ImGui::CalcTextSize(L("sActionHistoryDetails", "Result details")).x + ImGui::GetFrameHeight());
                            if (ImGui::TreeNode("##Details", "%s", L("sActionHistoryDetails", "Result details"))) {
                                ImGui::TextWrapped("%s", entry.details.c_str());
                                ImGui::TreePop();
                            }
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
                if (!displayed) ImGui::TextWrapped("%s", history.empty() ? L("sNoRecentActions", "No recent actions yet.") :
                    L("sNoMatchingActions", "No matching actions. Clear the search or turn off the attention filter."));
            }
            ImGui::EndChild();
        }
        ImGui::End();
        return position;
    }
}
