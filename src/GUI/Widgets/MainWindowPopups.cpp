#include "GUI/Widgets/MainWindowPopups.h"

#include "GUI/Widgets/ActionFeedback.h"
#include "GUI/Widgets/ImGuiWidgetUtils.h"
#include "GUI/Widgets/ModalUtils.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace ESPExplorerAE
{
    namespace
    {
        const char* ResolveString(const MainWindowPopups::LocalizeFn& localize,
            std::string_view section, std::string_view key, const char* fallback)
        {
            return localize ? localize(section, key, fallback) : fallback;
        }

        void ClosePopup(const char* id)
        {
            if (ImGui::BeginPopupModal(id)) {
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
    }

    void MainWindowPopups::RequestActionConfirmation(std::string title, std::string message,
        std::vector<ActionRequest> actions, Origin origin)
    {
        if (actions.empty()) return;
        confirmations.push_back({ std::move(title), std::move(message),
            { ++nextRevision, origin, std::move(actions) } });
    }

    void MainWindowPopups::OpenGlobalValuePopup(std::uint32_t formID, std::string editorID, std::uint64_t session)
    {
        globalValuePopup = { .formID = formID, .editorID = std::move(editorID), .session = session,
            .revision = ++nextRevision, .openRequested = true, .visible = true };
        closeGlobalRequested = false;
    }

    void MainWindowPopups::CloseConfirmation()
    {
        if (!confirmations.empty()) confirmations.pop_front();
        closeConfirmationRequested = true;
    }

    void MainWindowPopups::CloseGlobal()
    {
        globalValuePopup = {};
        closeGlobalRequested = true;
    }

    void MainWindowPopups::ResolveSubmit(std::uint64_t revision, ActionAdmission admission)
    {
        if (!confirmations.empty() && confirmations.front().pending && confirmations.front().submission.revision == revision) {
            auto& current = confirmations.front();
            current.pending = false;
            current.admission = admission;
            if (admission == ActionAdmission::Accepted) CloseConfirmation();
        } else if (globalValuePopup.pending && globalValuePopup.revision == revision) {
            globalValuePopup.pending = false;
            globalValuePopup.admission = admission;
            if (admission == ActionAdmission::Accepted) CloseGlobal();
        }
    }

    void MainWindowPopups::OpenHelpOverlay()
    {
        helpOverlay = { true, true };
    }

    void MainWindowPopups::OpenFirstRunHelpOverlay(bool dismissed)
    {
        if (!dismissed && !helpOverlay.visible && !helpOverlay.openRequested) OpenHelpOverlay();
    }

    void MainWindowPopups::HandleMenuVisibilityChanged(bool visible)
    {
        if (!visible) {
            confirmations.clear();
            closeConfirmationRequested = true;
            CloseGlobal();
        }
        if (helpOverlay.visible) helpOverlay.openRequested = true;
    }

    void MainWindowPopups::Draw(const View& view, Requests& requests)
    {
        const auto stale = [&](const Confirmation& confirmation) {
            return std::ranges::any_of(confirmation.submission.actions, [&](const ActionRequest& request) {
                return !request.session || request.session != view.session;
            });
        };
        if (!confirmations.empty() && stale(confirmations.front())) closeConfirmationRequested = true;
        std::erase_if(confirmations, stale);
        if (globalValuePopup.visible && globalValuePopup.session != view.session) CloseGlobal();

        if (closeConfirmationRequested) {
            ClosePopup("###ConfirmActionPopup");
            closeConfirmationRequested = false;
        } else RenderConfirmActionPopup(view, requests);
        if (closeGlobalRequested) {
            ClosePopup("###SetGlobalValuePopup");
            closeGlobalRequested = false;
        } else RenderGlobalValuePopup(view, requests);
        RenderHelpOverlay(view, requests);
    }

    void MainWindowPopups::RenderConfirmActionPopup(const View& view, Requests& requests)
    {
        if (confirmations.empty()) return;
        auto& current = confirmations.front();
        if (current.openRequested) {
            if (!ModalUtils::CanOpenPopup("###ConfirmActionPopup")) return;
            ImGui::OpenPopup("###ConfirmActionPopup");
            current.openRequested = false;
        }
        const float popupScale = (std::clamp)(view.fontSize / 20.0f, 0.75f, 1.5f);
        const ImVec2 initialSize(560.0f * popupScale, 300.0f * popupScale);
        const ModalUtils::PopupSizing popupSizing(initialSize,
            ImVec2(initialSize.x * 0.8f, initialSize.y * 0.8f),
            ImVec2(initialSize.x * 1.8f, initialSize.y * 2.4f), false);
        const auto popupTitle = std::string(ResolveString(view.localize, "General", "sConfirm", "Confirm")) + "###ConfirmActionPopup";
        if (!ImGui::BeginPopupModal(popupTitle.c_str(), &current.visible)) {
            if (!current.visible) CloseConfirmation();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            CloseConfirmation();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        const auto L = [&](auto section, auto key, auto fallback) { return ResolveString(view.localize, section, key, fallback); };
        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("##ConfirmationMessage", { 0, (std::max)(1.0f, ImGui::GetContentRegionAvail().y - footer) })) {
            ImGui::TextWrapped("%s", current.title.empty() ? L("General", "sConfirm", "Confirm") : current.title.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", current.message.c_str());
            ActionFeedback::Draw(current.admission, L);
        }
        ImGui::EndChild();
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        const bool canApply = view.gameplayReady && !current.pending;
        ImGui::BeginDisabled(!canApply);
        bool firstButton = true;
        const bool apply = ImGuiWidgetUtils::DrawWrappedButton(L("General", "sConfirm", "Confirm"), firstButton);
        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(view.gameplayReady,
            L("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open."));
        ImGui::EndDisabled();
        if (apply && canApply) {
            current.submission.revision = ++nextRevision;
            requests.submissions.push_back(current.submission);
            current.pending = true;
        }
        if (ImGuiWidgetUtils::DrawWrappedButton(L("General", "sCancel", "Cancel"), firstButton)) {
            CloseConfirmation();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void MainWindowPopups::RenderGlobalValuePopup(const View& view, Requests& requests)
    {
        if (!globalValuePopup.visible && !globalValuePopup.openRequested) return;
        if (globalValuePopup.openRequested) {
            if (!ModalUtils::CanOpenPopup("###SetGlobalValuePopup")) return;
            ImGui::OpenPopup("###SetGlobalValuePopup");
            globalValuePopup.openRequested = false;
        }
        const float popupScale = (std::clamp)(view.fontSize / 20.0f, 0.75f, 1.5f);
        const ImVec2 initialSize(480.0f * popupScale, 260.0f * popupScale);
        const ModalUtils::PopupSizing popupSizing(initialSize,
            ImVec2(initialSize.x * 0.8f, initialSize.y * 0.8f),
            ImVec2(initialSize.x * 1.8f, initialSize.y * 2.4f), false);
        const auto popupTitle = std::string(ResolveString(view.localize, "General", "sSetGlobal", "Set Global")) + "###SetGlobalValuePopup";
        if (!ImGui::BeginPopupModal(popupTitle.c_str(), &globalValuePopup.visible)) {
            if (!globalValuePopup.visible) CloseGlobal();
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            CloseGlobal();
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        const auto L = [&](auto section, auto key, auto fallback) { return ResolveString(view.localize, section, key, fallback); };
        ImGui::TextWrapped("%s: %s", L("General", "sEditorID", "EditorID"), globalValuePopup.editorID.c_str());
        ImGui::TextUnformatted(L("General", "sValue", "Value"));
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputFloat("##GlobalValue", &globalValuePopup.value, 1.0f, 10.0f, "%.3f");
        ImGui::Spacing();
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ActionFeedback::Draw(globalValuePopup.admission, L);
        const bool canApply = view.gameplayReady && std::isfinite(globalValuePopup.value) && !globalValuePopup.pending;
        ImGui::BeginDisabled(!canApply);
        bool firstButton = true;
        const bool apply = ImGuiWidgetUtils::DrawWrappedButton(L("General", "sApply", "Apply"), firstButton);
        ImGuiWidgetUtils::ShowGameplayDisabledTooltip(view.gameplayReady,
            L("General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open."));
        ImGui::EndDisabled();
        if (apply && canApply) {
            requests.submissions.push_back({ globalValuePopup.revision = ++nextRevision, Origin::Global,
                { { .kind = ActionKind::SetGlobal, .formID = globalValuePopup.formID,
                    .session = globalValuePopup.session, .value = globalValuePopup.value } } });
            globalValuePopup.pending = true;
        }
        if (ImGuiWidgetUtils::DrawWrappedButton(L("General", "sCancel", "Cancel"), firstButton)) {
            CloseGlobal();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void MainWindowPopups::RenderHelpOverlay(const View& view, Requests& requests)
    {
        if (!helpOverlay.visible && !helpOverlay.openRequested) return;
        if (helpOverlay.openRequested) {
            if (!ModalUtils::CanOpenPopup("##HelpOverlayPopup")) return;
            ImGui::OpenPopup("##HelpOverlayPopup");
            helpOverlay.openRequested = false;
        }

        const float popupScale = (std::clamp)(view.fontSize / 20.0f, 0.75f, 1.5f);
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float maxWidth = (std::max)(420.0f, viewport->WorkSize.x - 48.0f);
        const float maxHeight = (std::max)(320.0f, viewport->WorkSize.y - 48.0f);
        const ImVec2 initialSize(
            (std::min)(720.0f * popupScale, maxWidth),
            (std::min)(640.0f * popupScale, maxHeight));
        const ModalUtils::PopupSizing popupSizing(
            initialSize,
            ImVec2(initialSize.x, initialSize.y),
            ImVec2(initialSize.x, initialSize.y));
        if (!ImGui::BeginPopupModal("##HelpOverlayPopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
            return;
        }

        const auto toggleKeyName = view.toggleKeyName;
        const auto toggleHelp = ResolveString(view.localize, "Settings", "sToggleKey", "Toggle Key");
        const auto helpTitle = ResolveString(view.localize, "General", "sHelpOverlayTitle", "Getting Started");
        const auto closeLabel = ResolveString(view.localize, "General", "sCloseHelpOverlay", "Start Exploring");
        ImGui::TextUnformatted(helpTitle);
        ImGui::Separator();

        const float buttonHeight = ImGui::GetFrameHeightWithSpacing();
        const float contentHeight = (std::max)(1.0f, ImGui::GetContentRegionAvail().y - buttonHeight - ImGui::GetStyle().ItemSpacing.y * 2.0f);
        if (ImGui::BeginChild("##HelpOverlayContent", ImVec2(0.0f, contentHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextDisabled("%s", ResolveString(view.localize, "General", "sHelpOverlayHotkeys", "Hotkeys"));
            ImGuiWidgetUtils::DrawWrappedBullet(std::string(toggleHelp) + ": " + toggleKeyName);
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayFocusSearch", "Ctrl+F: Focus the active search field"));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayHotkeysBody", "Press the toggle key to open or close the menu."));

            ImGui::Spacing();
            ImGui::TextDisabled("%s", ResolveString(view.localize, "General", "sHelpOverlayFilters", "Filters"));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayFiltersBody", "Plugin filters choose sources; record filters control which records appear."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlaySearchBody", "Search all plugins, selected plugins, or a collection using the scope menu."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayRuntimeRecordsBody", "Shows loaded game data, which may differ from xEdit."));

            ImGui::Spacing();
            ImGui::TextDisabled("%s", ResolveString(view.localize, "General", "sHelpOverlayFavorites", "Favorites And Recent"));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayFavoritesBody", "Right-click a record to save it as a favorite."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayRecentBody", "Recent Records lets you revisit recently inspected records."));

            ImGui::Spacing();
            ImGui::TextDisabled("%s", ResolveString(view.localize, "General", "sHelpOverlayAdvancedFilters", "Advanced Filters"));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayAdvancedFiltersBody", "Use Advanced Filters to hide matching records across browsers. Rules can target specific plugins."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayAdvancedFiltersExampleBody", "Example: hide records containing 'SS2_Tag_'."));

            ImGui::Spacing();
            ImGui::TextDisabled("%s", ResolveString(view.localize, "General", "sHelpOverlayInventory", "Inventory Tab"));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayInventoryBody", "Filter and sort your inventory. Choose an instance to equip or use; open Actions to change quantities, drop, or remove."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayInventoryComponentBody", "Components are given as crafting scrap. Change this in Settings > Gameplay."));

            ImGui::Spacing();
            ImGui::TextDisabled("%s", ResolveString(view.localize, "General", "sHelpOverlaySafeActions", "Safe Actions"));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlaySafeActionsBody", "Viewing details, copying IDs, and filtering are safe. Give, spawn, and teleport actions are explicit and important actions ask for confirmation."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayMainMenuActionsBody", "Main menu gameplay actions are disabled by default; enabling them may cause crashes."));
            ImGuiWidgetUtils::DrawWrappedBullet(ResolveString(view.localize, "General", "sHelpOverlayHistoryBody", "Action History shows recent requests and their observed results."));
            ImGui::PopTextWrapPos();
        }
        ImGui::EndChild();

        ImGui::Spacing();
        if (ImGui::Button(closeLabel, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            requests.helpDismissed = true;
            helpOverlay = {};
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
