#include "GUI/Widgets/RecordInspector.h"

#include "Core/RecordActions.h"
#include "Core/Workspace.h"
#include "GUI/Widgets/BrowserWidgets.h"
#include "GUI/Widgets/FormatUtils.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "Input/GamepadInput.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    void DrawRecordInspector(RecordInspectorState& state, const BrowserView& view,
        std::shared_ptr<const RecordDetails> details, bool advanced, bool outsideScope, RecordInspectorRequests& requests)
    {
        const auto& localize = view.localize;
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !GamepadInput::IsSteamKeyboardOpen() && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
            !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
            if (ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false) && state.history.CanBack()) requests.historyMove = -1;
            if (ImGui::GetIO().KeyAlt && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false) && state.history.CanForward()) requests.historyMove = 1;
        }
        bool first = true;
        ImGui::BeginDisabled(!state.history.CanBack());
        if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sBack", "Back"), first)) requests.historyMove = -1;
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!state.history.CanForward());
        if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sForward", "Forward"), first)) requests.historyMove = 1;
        ImGui::EndDisabled();
        auto* location = state.history.Current();
        const auto* record = location && view.catalog ? view.catalog->Find(location->formID) : nullptr;
        if (!record) {
            ImGui::TextWrapped("%s", localize("PluginBrowser", "sSelectRecordHint", "Select a record to view details."));
            return;
        }
        ImGui::BeginDisabled(!state.canPin);
        if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sPin", "Pin"), first)) requests.pins.push_back(record->formID);
        ImGui::EndDisabled();
        if (!state.canPin && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", localize("FormDetails", "sPinLimit", "Three inspectors are pinned. Close one before pinning another."));
        first = true;
        if (SupportsRecordAction(record->category, ActionKind::Give)) {
            ImGui::BeginDisabled(!PrepareRecordAction(ActionKind::Give, *record, view.session, view.gameplayReady));
            if (ImGuiWidgetUtils::DrawWrappedButton(localize("Items", "sGiveItem", "Give Item"), first)) requests.records.grants.push_back({view.session, {record->formID}});
            ImGui::EndDisabled();
        }
        const auto popup = "InspectorActions/" + std::to_string(record->formID);
        if (ImGuiWidgetUtils::DrawWrappedButton(localize("General", "sActions", "Actions"), first)) ImGui::OpenPopup(popup.c_str());
        if (ImGui::BeginPopup(popup.c_str())) {
            if (ImGui::MenuItem(localize("General", "sCopyIdentity", "Copy Identity"))) {
                const auto target = CaptureWorkspaceRecord(*view.catalog, record->formID, view.session);
                ImGui::SetClipboardText((target.identity.empty() ? std::string(localize("Workspace", "sSessionOnly", "Session only; not restored as a target")) + ": " + FormatUtils::FormID(record->formID) : target.identity).c_str());
            }
            BrowserWidgets::DrawContext(*record, BrowserWidgets::ContextScope::Single, state.quantities, view, requests.records);
            ImGui::EndPopup();
        }
        ImGuiWidgetUtils::DrawStatusArea("##InspectorFeedback", [&] {
            if (outsideScope) ImGui::TextWrapped("%s", localize("FormDetails", "sOutsideScope", "This record is outside the current results."));
        });
        ImGui::Separator();
        if (state.restoreScroll) ImGui::SetNextWindowScroll({0.0f, location->scroll});
        if (ImGui::BeginChild("InspectorInformation", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            state.restoreScroll = false;
            location->scroll = ImGui::GetScrollY();
            FormDetailsView::Draw(*record, {localize, advanced, view.catalog.get(), std::move(details),
                [&](auto id) { requests.open = id; }, [&](auto id) { requests.pins.push_back(id); }, [&](auto id) { requests.records.collections.push_back(id); }, state.canPin});
        }
        ImGui::EndChild();
    }
}
