#include "GUI/Widgets/MainWindowPopups.h"

#include "Config/Config.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/ModalUtils.h"
#include "Localization/Language.h"
#include "Logging/Logger.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include <RE/T/TESForm.h>

namespace ESPExplorerAE::MainWindowPopups
{
    namespace
    {
        struct ConfirmActionState
        {
            bool openRequested{ false };
            bool visible{ false };
            std::string title{};
            std::string message{};
            std::function<void()> callback{};
        };

        struct GlobalValuePopupState
        {
            bool openRequested{ false };
            bool visible{ false };
            std::uint32_t formID{ 0 };
            std::string editorID{};
            float value{ 0.0f };
        };

        struct HelpOverlayState
        {
            bool openRequested{ false };
            bool visible{ false };
            bool persistDismissal{ false };
        };

        ConfirmActionState confirmAction{};
        GlobalValuePopupState globalValuePopup{};
        HelpOverlayState helpOverlay{};

        const char* ResolveString(const LocalizeFn& localize, std::string_view section, std::string_view key, const char* fallback)
        {
            if (localize) {
                return localize(section, key, fallback);
            }

            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
        }

        std::string KeyNameFromVK(std::uint32_t vk)
        {
            const auto scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
            const LONG keyData = static_cast<LONG>(static_cast<LPARAM>(scanCode) << 16);

            char keyName[128]{};
            if (GetKeyNameTextA(keyData, keyName, static_cast<int>(std::size(keyName))) > 0) {
                return std::string(keyName);
            }

            return std::to_string(vk);
        }

        void DismissHelpOverlay()
        {
            if (helpOverlay.persistDismissal) {
                auto& settings = Config::GetMutable();
                if (!settings.firstRunHelpDismissed) {
                    settings.firstRunHelpDismissed = true;
                    Config::RequestSave();
                }
            }

            helpOverlay.visible = false;
            helpOverlay.persistDismissal = false;
            Logger::Verbose("Help overlay dismissed");
        }

        void RenderConfirmActionPopup(const LocalizeFn& localize)
        {
            if (confirmAction.openRequested) {
                ImGui::OpenPopup("##ConfirmActionPopup");
                confirmAction.openRequested = false;
            }

            const float popupScale = (std::clamp)(Config::Get().fontSize / 20.0f, 0.75f, 1.5f);
            const ImVec2 initialSize(460.0f * popupScale, 180.0f * popupScale);
            ModalUtils::SetNextPopupWindowSizing(
                initialSize,
                ImVec2(initialSize.x * 0.8f, initialSize.y * 0.8f),
                ImVec2(initialSize.x * 1.8f, initialSize.y * 1.8f));
            if (!ImGui::BeginPopupModal("##ConfirmActionPopup", &confirmAction.visible)) {
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                confirmAction.callback = {};
                confirmAction.visible = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            ImGui::TextUnformatted(confirmAction.title.empty() ? ResolveString(localize, "General", "sConfirm", "") : confirmAction.title.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", confirmAction.message.c_str());
            ImGui::Spacing();

            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            if (ImGui::Button(ResolveString(localize, "General", "sConfirm", "Confirm"), ImVec2(110.0f, 0.0f))) {
                if (confirmAction.callback) {
                    confirmAction.callback();
                }
                confirmAction.callback = {};
                confirmAction.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(ResolveString(localize, "General", "sCancel", "Cancel"), ImVec2(110.0f, 0.0f))) {
                confirmAction.callback = {};
                confirmAction.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        void RenderGlobalValuePopup(const LocalizeFn& localize)
        {
            if (globalValuePopup.openRequested) {
                ImGui::OpenPopup("##SetGlobalValuePopup");
                globalValuePopup.openRequested = false;
            }

            const float popupScale = (std::clamp)(Config::Get().fontSize / 20.0f, 0.75f, 1.5f);
            const ImVec2 initialSize(430.0f * popupScale, 180.0f * popupScale);
            ModalUtils::SetNextPopupWindowSizing(
                initialSize,
                ImVec2(initialSize.x * 0.8f, initialSize.y * 0.8f),
                ImVec2(initialSize.x * 1.8f, initialSize.y * 1.8f));
            if (!ImGui::BeginPopupModal("##SetGlobalValuePopup", &globalValuePopup.visible)) {
                return;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }

            ImGui::TextUnformatted(ResolveString(localize, "General", "sSetGlobal", "Set Global"));
            ImGui::Separator();
            ImGui::Text("%s: %s", ResolveString(localize, "General", "sEditorID", "EditorID"), globalValuePopup.editorID.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputFloat(ResolveString(localize, "General", "sValue", "Value"), &globalValuePopup.value, 1.0f, 10.0f, "%.3f");
            ImGui::Spacing();

            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            const bool gameplayActionsAllowed = FormActions::AreGameplayActionsAllowed();
            if (!gameplayActionsAllowed) {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button(ResolveString(localize, "General", "sApply", "Apply"), ImVec2(100.0f, 0.0f))) {
                char command[256]{};
                std::snprintf(command, sizeof(command), "set %s to %.3f", globalValuePopup.editorID.c_str(), globalValuePopup.value);
                FormActions::ExecuteConsoleCommand(command);
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
            }
            if (!gameplayActionsAllowed && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", ResolveString(localize, "General", "sGameplayActionsDisabledInMainMenu", "Gameplay actions are disabled while the main menu is open."));
            }
            if (!gameplayActionsAllowed) {
                ImGui::EndDisabled();
            }

            ImGui::SameLine();
            if (ImGui::Button(ResolveString(localize, "General", "sCancel", "Cancel"), ImVec2(100.0f, 0.0f))) {
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        void RenderHelpOverlay(const LocalizeFn& localize)
        {
            if (helpOverlay.openRequested) {
                ImGui::OpenPopup("##HelpOverlayPopup");
                helpOverlay.openRequested = false;
            }

            const float popupScale = (std::clamp)(Config::Get().fontSize / 20.0f, 0.75f, 1.5f);
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            const float maxWidth = (std::max)(420.0f, viewport->WorkSize.x - 48.0f);
            const float maxHeight = (std::max)(320.0f, viewport->WorkSize.y - 48.0f);
            const ImVec2 initialSize(
                (std::min)(720.0f * popupScale, maxWidth),
                (std::min)(640.0f * popupScale, maxHeight));
            ModalUtils::SetNextPopupWindowSizing(
                initialSize,
                ImVec2(initialSize.x, initialSize.y),
                ImVec2(initialSize.x, initialSize.y));
            if (!ImGui::BeginPopupModal("##HelpOverlayPopup", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar)) {
                return;
            }

            const auto toggleKeyName = KeyNameFromVK(Config::Get().toggleKey);
            const auto toggleHelp = ResolveString(localize, "Settings", "sToggleKey", "Toggle Key");
            const auto helpTitle = ResolveString(localize, "General", "sHelpOverlayTitle", "Getting Started");
            const auto closeLabel = ResolveString(localize, "General", "sCloseHelpOverlay", "Start Exploring");
            const auto drawWrappedBullet = [](std::string_view text) {
                ImGui::Bullet();
                ImGui::SameLine();
                const float wrapPos = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                ImGui::PushTextWrapPos(wrapPos);
                ImGui::TextUnformatted(text.data(), text.data() + text.size());
                ImGui::PopTextWrapPos();
            };

            ImGui::TextUnformatted(helpTitle);
            ImGui::Separator();

            const float buttonHeight = ImGui::GetFrameHeightWithSpacing();
            const float contentHeight = (std::max)(120.0f, ImGui::GetContentRegionAvail().y - buttonHeight - ImGui::GetStyle().ItemSpacing.y * 2.0f);
            if (ImGui::BeginChild("##HelpOverlayContent", ImVec2(0.0f, contentHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
                ImGui::TextDisabled("%s", ResolveString(localize, "General", "sHelpOverlayHotkeys", "Hotkeys"));
                drawWrappedBullet(std::string(toggleHelp) + ": " + toggleKeyName);
                drawWrappedBullet(std::string("Ctrl+Z: ") + ResolveString(localize, "General", "sUndoLastAction", "Undo Last Action"));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayHotkeysBody", "Use the configured toggle key to open or close the menu at any time."));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", ResolveString(localize, "General", "sHelpOverlayFilters", "Filters"));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayFiltersBody", "Plugin filters narrow the left tree, while record filters hide or include playable, unnamed, deleted, and unknown records."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlaySearchBody", "Global Search scans every loaded record. Turn it off when you want to stay inside the active plugin filter."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayRuntimeRecordsBody", "ESP Explorer AE reads records after Fallout 4 has loaded and resolved them at runtime. Some values may differ from raw plugin data in xEdit."));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", ResolveString(localize, "General", "sHelpOverlayFavorites", "Favorites And Recent"));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayFavoritesBody", "Add favorites from record actions or context menus to pin important forms across sessions."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayRecentBody", "Recent Records tracks what you inspect most often, making it easy to jump back without searching again."));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", ResolveString(localize, "General", "sHelpOverlayAdvancedFilters", "Advanced Filters"));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayAdvancedFiltersBody", "Advanced Record Filters let you define keyword-based rules to block or allow specific records globally. Access them from the filter toolbar or Settings."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayAdvancedFiltersExampleBody", "For example, you can hide all records containing 'SS2_Tag_' to declutter Sim Settlements content from your results."));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", ResolveString(localize, "General", "sHelpOverlayInventory", "Inventory Tab"));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayInventoryBody", "The Inventory tab shows your current player inventory grouped by category. You can inspect, drop, or favorite items directly from it."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayInventoryComponentBody", "Components are automatically substituted to their usable scrap form when given, so they work for crafting. This can be toggled in Settings > Gameplay."));

                ImGui::Spacing();
                ImGui::TextDisabled("%s", ResolveString(localize, "General", "sHelpOverlaySafeActions", "Safe Actions"));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlaySafeActionsBody", "Viewing details, copying IDs, and filtering are safe. Give, spawn, and teleport actions are explicit and important actions ask for confirmation."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayMainMenuActionsBody", "Gameplay actions stay disabled in the main menu by default for stability. Only enable them if you understand the risks and want that behavior."));
                drawWrappedBullet(ResolveString(localize, "General", "sHelpOverlayHistoryBody", "The Action History panel in the status bar shows recent give, spawn, and teleport actions, with one-click undo when the action can be reversed."));
                ImGui::PopTextWrapPos();
            }
            ImGui::EndChild();

            ImGui::Spacing();
            if (ImGui::Button(closeLabel, ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                DismissHelpOverlay();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void RequestActionConfirmation(std::string title, std::string message, std::function<void()> callback)
    {
        Logger::Verbose("Queued confirmation popup: " + title);
        confirmAction.title = std::move(title);
        confirmAction.message = std::move(message);
        confirmAction.callback = std::move(callback);
        confirmAction.openRequested = true;
        confirmAction.visible = true;
    }

    void OpenGlobalValuePopup(std::uint32_t formID)
    {
        if (!FormActions::AreGameplayActionsAllowed()) {
            Logger::Verbose("Blocked Set Global popup because gameplay actions are disabled");
            return;
        }

        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            Logger::Warn("Set Global popup requested for missing form");
            return;
        }

        const auto* editorID = form->GetFormEditorID();
        if (!editorID || editorID[0] == '\0') {
            Logger::Warn("Set Global popup requested for form without editor ID");
            return;
        }

        globalValuePopup.formID = formID;
        globalValuePopup.editorID = editorID;
        globalValuePopup.value = 0.0f;
        globalValuePopup.openRequested = true;
        globalValuePopup.visible = true;
        Logger::Verbose("Opened Set Global popup for editor ID " + globalValuePopup.editorID);
    }

    void OpenHelpOverlay()
    {
        helpOverlay.persistDismissal = true;
        helpOverlay.openRequested = true;
        helpOverlay.visible = true;
        Logger::Verbose("Help overlay requested manually");
    }

    void OpenFirstRunHelpOverlay()
    {
        if (helpOverlay.visible || helpOverlay.openRequested || Config::Get().firstRunHelpDismissed) {
            return;
        }

        helpOverlay.persistDismissal = true;
        helpOverlay.openRequested = true;
        helpOverlay.visible = true;
        Logger::Verbose("Help overlay requested for first run");
    }

    void HandleMenuVisibilityChanged(bool visible)
    {
        if (!visible) {
            if (confirmAction.visible) {
                confirmAction.visible = false;
                confirmAction.openRequested = false;
                confirmAction.callback = {};
                Logger::Verbose("Closed confirm action popup on menu hide");
            }
            if (globalValuePopup.visible) {
                globalValuePopup.visible = false;
                globalValuePopup.openRequested = false;
                Logger::Verbose("Closed global value popup on menu hide");
            }
        }

        if (helpOverlay.visible) {
            helpOverlay.openRequested = true;
            Logger::Verbose("Reopening help overlay after menu visibility change");
        }
    }

    void Draw(const LocalizeFn& localize)
    {
        RenderConfirmActionPopup(localize);
        RenderGlobalValuePopup(localize);
        RenderHelpOverlay(localize);
    }
}
