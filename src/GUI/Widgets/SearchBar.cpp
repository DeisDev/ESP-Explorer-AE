#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    bool SearchBar::Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value)
    {
        if (!label || !buffer || bufferSize == 0) {
            return false;
        }

        bool changed = false;

        if (ImGui::InputText(label, buffer, bufferSize)) {
            changed = true;
        }

        ImGui::SameLine();
        const std::string clearLabel = std::string("Clear##") + label;
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
