#include "GUI/Widgets/SearchBar.h"

#include "Input/GamepadInput.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    bool SearchBar::Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value, bool* shouldFocus, const char* stableId, const char* clearText)
    {
        if (!label || !buffer || bufferSize == 0) {
            return false;
        }

        bool changed = false;

        if (shouldFocus && *shouldFocus && !ImGui::IsAnyItemActive() && !GamepadInput::IsUsingGamepad()) {
            ImGui::SetKeyboardFocusHere();
            *shouldFocus = false;
        }

        ImGui::SetNextItemWidth(-ImGui::CalcTextSize(clearText).x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::InputTextWithHint((std::string("##") + (stableId ? stableId : label)).c_str(), label, buffer, bufferSize)) {
            changed = true;
        }
        if (ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && buffer[0] != '\0') {
            buffer[0] = '\0';
            value.clear();
            changed = true;
        }

        if (ImGui::IsItemActivated() && GamepadInput::IsUsingGamepad() && !GamepadInput::IsSteamKeyboardOpen()) {
            GamepadInput::ShowSteamKeyboard(ImGui::GetItemID(), label, buffer, bufferSize);
        }

        if (GamepadInput::IsSteamKeyboardOpen()) {
            changed |= GamepadInput::CheckSteamKeyboardResult(ImGui::GetItemID(), buffer, bufferSize, value);
        }

        ImGui::SameLine();
        const std::string clearLabel = std::string(clearText) + "##Clear" + (stableId ? stableId : label);
        if (ImGui::Button(clearLabel.c_str())) {
            buffer[0] = '\0';
            value.clear();
            changed = true;
        }

        if (changed) {
            value = buffer;
        }

        return changed;
    }
}
