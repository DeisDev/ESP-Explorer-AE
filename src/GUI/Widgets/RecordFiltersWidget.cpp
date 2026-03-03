#include "GUI/Widgets/RecordFiltersWidget.h"

#include <imgui.h>

namespace ESPExplorerAE
{
    bool RecordFiltersWidget::Draw(const LocalizeFn& localize, std::string_view idSuffix, RecordFilterState state)
    {
        bool changed = false;

        const std::string headerLabel = std::string(localize("General", "sRecordFilters", "Record Filters")) + "##" + std::string(idSuffix);
        const ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
        if (!ImGui::TreeNodeEx(headerLabel.c_str(), headerFlags)) {
            return false;
        }

        const std::string tableId = "RecordFiltersTable##" + std::string(idSuffix);
        if (ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string playableLabel = std::string(localize("General", "sPlayable", "Playable")) + "##Playable" + std::string(idSuffix);
            if (ImGui::Checkbox(playableLabel.c_str(), &state.showPlayable)) {
                changed = true;
            }

            ImGui::TableNextColumn();
            const std::string nonPlayableLabel = std::string(localize("General", "sNonPlayable", "Non-Playable")) + "##NonPlayable" + std::string(idSuffix);
            if (ImGui::Checkbox(nonPlayableLabel.c_str(), &state.showNonPlayable)) {
                changed = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string namedLabel = std::string(localize("General", "sNamed", "Named")) + "##Named" + std::string(idSuffix);
            if (ImGui::Checkbox(namedLabel.c_str(), &state.showNamed)) {
                changed = true;
            }

            ImGui::TableNextColumn();
            const std::string unnamedLabel = std::string(localize("General", "sUnnamedLabel", "Unnamed")) + "##Unnamed" + std::string(idSuffix);
            if (ImGui::Checkbox(unnamedLabel.c_str(), &state.showUnnamed)) {
                changed = true;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string deletedLabel = std::string(localize("General", "sDeleted", "Deleted")) + "##Deleted" + std::string(idSuffix);
            if (ImGui::Checkbox(deletedLabel.c_str(), &state.showDeleted)) {
                changed = true;
            }

            ImGui::EndTable();
        }

        if (!state.showPlayable && !state.showNonPlayable) {
            ImGui::TextDisabled("%s", localize("General", "sFilterNoPlayableSelected", "No playable state selected."));
        }
        if (!state.showNamed && !state.showUnnamed) {
            ImGui::TextDisabled("%s", localize("General", "sFilterNoNameSelected", "No name state selected."));
        }

        ImGui::TreePop();

        return changed;
    }
}
