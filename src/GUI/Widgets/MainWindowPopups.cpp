#include "GUI/Widgets/MainWindowPopups.h"

#include "Config/Config.h"
#include "GUI/Widgets/FormActions.h"
#include "GUI/Widgets/ModalUtils.h"
#include "Localization/Language.h"

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

        ConfirmActionState confirmAction{};
        GlobalValuePopupState globalValuePopup{};

        const char* ResolveString(const LocalizeFn& localize, std::string_view section, std::string_view key, const char* fallback)
        {
            if (localize) {
                return localize(section, key, fallback);
            }

            const auto value = Language::Get(section, key);
            return value.empty() ? fallback : value.data();
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

            ImGui::TextUnformatted(ResolveString(localize, "General", "sSetGlobal", "Set Global"));
            ImGui::Separator();
            ImGui::Text("%s: %s", ResolveString(localize, "General", "sEditorID", "EditorID"), globalValuePopup.editorID.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputFloat(ResolveString(localize, "General", "sValue", "Value"), &globalValuePopup.value, 1.0f, 10.0f, "%.3f");
            ImGui::Spacing();

            if (ImGui::IsWindowAppearing()) {
                ImGui::SetKeyboardFocusHere();
            }
            if (ImGui::Button(ResolveString(localize, "General", "sApply", "Apply"), ImVec2(100.0f, 0.0f))) {
                char command[256]{};
                std::snprintf(command, sizeof(command), "set %s to %.3f", globalValuePopup.editorID.c_str(), globalValuePopup.value);
                FormActions::ExecuteConsoleCommand(command);
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button(ResolveString(localize, "General", "sCancel", "Cancel"), ImVec2(100.0f, 0.0f))) {
                globalValuePopup.visible = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void RequestActionConfirmation(std::string title, std::string message, std::function<void()> callback)
    {
        confirmAction.title = std::move(title);
        confirmAction.message = std::move(message);
        confirmAction.callback = std::move(callback);
        confirmAction.openRequested = true;
        confirmAction.visible = true;
    }

    void OpenGlobalValuePopup(std::uint32_t formID)
    {
        auto* form = RE::TESForm::GetFormByID(formID);
        if (!form) {
            return;
        }

        const auto* editorID = form->GetFormEditorID();
        if (!editorID || editorID[0] == '\0') {
            return;
        }

        globalValuePopup.formID = formID;
        globalValuePopup.editorID = editorID;
        globalValuePopup.value = 0.0f;
        globalValuePopup.openRequested = true;
        globalValuePopup.visible = true;
    }

    void Draw(const LocalizeFn& localize)
    {
        RenderConfirmActionPopup(localize);
        RenderGlobalValuePopup(localize);
    }
}