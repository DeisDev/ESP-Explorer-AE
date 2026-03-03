#include "GUI/Widgets/SearchBar.h"

#include "Input/GamepadInput.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    bool SearchBar::Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value)
    {
        if (!label || !buffer || bufferSize == 0) {
            return false;
        }

        bool changed = false;

        ImGui::SetNextItemWidth(-ImGui::CalcTextSize("X").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::InputTextWithHint(std::string(std::string("##") + label).c_str(), label, buffer, bufferSize)) {
            changed = true;
        }

        if (ImGui::IsItemActivated() && GamepadInput::IsUsingGamepad()) {
            GamepadInput::ShowSteamKeyboard();
        }

        if (GamepadInput::IsSteamKeyboardOpen() && ImGui::IsItemActive()) {
            GamepadInput::CheckSteamKeyboardResult(buffer, bufferSize, value);
            changed = true;
        }

        ImGui::SameLine();
        const std::string clearLabel = std::string("X##") + label;
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
