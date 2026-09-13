#include "GUI/Widgets/CommandPalette.h"
#include "GUI/Widgets/ModalUtils.h"
#include "GUI/Widgets/SearchBar.h"
#include "Input/GamepadInput.h"
#include <imgui.h>

namespace ESPExplorerAE
{
    std::optional<WorkspaceCommand> DrawCommandPalette(CommandPaletteState& state, const std::vector<WorkspaceCommand>& commands, const BrowserView& view)
    {
        const auto& localize = view.localize;
        const auto title = std::string(localize("Workspace", "sCommandPalette", "Commands & Shortcuts")) + "###WorkspacePalette";
        if (state.requested && ModalUtils::CanOpenPopup(title.c_str()) && !GamepadInput::IsSteamKeyboardOpen()) {
            ImGui::OpenPopup(title.c_str()); state.requested = false; state.focus = true; state.selected = 0;
        }
        const auto* viewport = ImGui::GetMainViewport();
        const ModalUtils::PopupSizing sizing({680, 520}, {360, 260}, {viewport->WorkSize.x - 24, viewport->WorkSize.y - 24}, false);
        std::optional<WorkspaceCommand> chosen;
        bool open = true;
        if (ImGui::BeginPopupModal(title.c_str(), &open, ImGuiWindowFlags_NoSavedSettings)) {
            if (!GamepadInput::IsSteamKeyboardOpen() && ModalUtils::CancelPopupRequested()) ImGui::CloseCurrentPopup();
            if (SearchBar::Draw(localize("Workspace", "sCommandSearch", "Search commands, bindings, saved views, or enter an 8-digit FormID"), state.buffer.data(), state.buffer.size(),
                state.search, &state.focus, "PaletteSearch", localize("General", "sClearSearchButton", "X"))) state.selected = 0;
            auto available = commands;
            if (const auto id = ParseExactFormID(state.search)) available.insert(available.begin(), {WorkspaceCommandKind::Inspect,
                std::string(localize("General", "sInspect", "Inspect")) + " " + state.search, "Enter", state.search, view.catalog->Find(*id) != nullptr,
                localize("FormDetails", "sTargetUnavailable", "Record not found in the loaded data.")});
            const auto matches = MatchWorkspaceCommands(available, state.search);
            state.selected = matches.empty() ? 0 : (std::min)(state.selected, matches.size() - 1);
            const bool keyboard = !GamepadInput::IsSteamKeyboardOpen();
            if (keyboard && ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) && state.selected + 1 < matches.size()) ++state.selected;
            if (keyboard && ImGui::IsKeyPressed(ImGuiKey_UpArrow, true) && state.selected) --state.selected;
            if (ImGui::BeginChild("PaletteCommands", {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened)) {
                for (std::size_t row = 0; row < matches.size(); ++row) {
                    const auto& command = available[matches[row]];
                    ImGui::PushID(static_cast<int>(matches[row]));
                    ImGui::BeginDisabled(!command.enabled);
                    if (ImGui::Selectable((command.label + "###Command").c_str(), row == state.selected)) chosen = command;
                    ImGui::EndDisabled();
                    if (!command.binding.empty()) { ImGui::SameLine(); ImGui::TextDisabled("%s", command.binding.c_str()); }
                    if (!command.enabled && !command.explanation.empty()) ImGui::TextWrapped("%s", command.explanation.c_str());
                    if (row == state.selected && keyboard && (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true) || ImGui::IsKeyPressed(ImGuiKey_UpArrow, true))) ImGui::SetScrollHereY();
                    ImGui::PopID();
                }
                if (matches.empty()) ImGui::TextWrapped("%s", localize("Workspace", "sNoCommands", "No matching commands."));
            }
            ImGui::EndChild();
            if (keyboard && !matches.empty() && ImGui::IsKeyPressed(ImGuiKey_Enter, false) && available[matches[state.selected]].enabled) chosen = available[matches[state.selected]];
            if (chosen) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        return chosen;
    }
}
