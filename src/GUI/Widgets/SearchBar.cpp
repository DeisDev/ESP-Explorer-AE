#include "GUI/Widgets/SearchBar.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    bool SearchBar::Draw(const char* label, char* buffer, std::size_t bufferSize, std::string& value)
    {
        if (!label || !buffer || bufferSize == 0) {
            return false;
        }

        if (!ImGui::InputText(label, buffer, bufferSize)) {
            return false;
        }

        value = buffer;
        return true;
    }
}
