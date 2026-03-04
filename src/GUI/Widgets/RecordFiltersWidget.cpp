#include "GUI/Widgets/RecordFiltersWidget.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    bool RecordFiltersWidget::Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state)
    {
        bool changed = false;

        const std::string nonPlayableLabel = std::string(localize("General", "sIncludeNonPlayable", "Include Non-Playable")) + "##NonPlayable" + std::string(idSuffix);
        if (ImGui::Checkbox(nonPlayableLabel.c_str(), &state.showNonPlayable)) {
            changed = true;
        }

        ImGui::SameLine();
        const std::string unnamedLabel = std::string(localize("General", "sIncludeUnnamed", "Include Unnamed")) + "##Unnamed" + std::string(idSuffix);
        if (ImGui::Checkbox(unnamedLabel.c_str(), &state.showUnnamed)) {
            changed = true;
        }

        ImGui::SameLine();
        const std::string deletedLabel = std::string(localize("General", "sIncludeDeleted", "Include Deleted")) + "##Deleted" + std::string(idSuffix);
        if (ImGui::Checkbox(deletedLabel.c_str(), &state.showDeleted)) {
            changed = true;
        }

        return changed;
    }
}
