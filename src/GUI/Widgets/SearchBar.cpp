#include "GUI/Widgets/SearchBar.h"

#include "Input/GamepadInput.h"

#include <imgui.h>
#include <algorithm>

namespace ESPExplorerAE
{
    bool SearchBar::ReadControllerText(const char* label, char* buffer, std::size_t bufferSize)
    {
        if (ImGui::IsItemActivated() && GamepadInput::IsUsingGamepad() && !GamepadInput::IsSteamKeyboardOpen())
            GamepadInput::ShowSteamKeyboard(ImGui::GetItemID(), label, buffer, bufferSize);
        std::string result;
        return GamepadInput::IsSteamKeyboardOpen() && GamepadInput::CheckSteamKeyboardResult(ImGui::GetItemID(), buffer, bufferSize, result);
    }

    bool SearchBar::Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value, bool* shouldFocus, const char* stableId, const char* clearText, float width)
    {
        if (!label || !buffer || bufferSize == 0) {
            return false;
        }

        bool changed = false;

        if (shouldFocus && *shouldFocus && !ImGui::IsAnyItemActive() && !GamepadInput::IsUsingGamepad()) {
            ImGui::SetKeyboardFocusHere();
            *shouldFocus = false;
        }

        const float available = width > 0 ? width : ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth((std::max)(1.0f, available - ImGui::CalcTextSize(clearText).x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x));
        if (ImGui::InputTextWithHint((std::string("##") + (stableId ? stableId : label)).c_str(), label, buffer, bufferSize, ImGuiInputTextFlags_EscapeClearsAll)) {
            changed = true;
        }

        changed |= ReadControllerText(label, buffer, bufferSize);

        ImGui::SameLine();
        const std::string clearLabel = std::string(clearText) + "##Clear" + (stableId ? stableId : label);
        if (ImGui::Button(clearLabel.c_str())) {
            buffer[0] = '\0';
            value.clear();
            changed = true;
            if (shouldFocus) *shouldFocus = true;
        }

        if (changed) {
            value = buffer;
        }

        return changed;
    }
}
