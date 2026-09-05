#include "GUI/Tabs/LogViewerTab.h"

#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace ESPExplorerAE
{
    namespace
    {
        void CopySelectedLines(const LogViewerState& state, const LogSnapshot& view)
        {
            if (state.selectedLineIndexes.empty()) {
                return;
            }

            std::string text;
            for (std::size_t index = 0; index < view.lines.size(); ++index) {
                if (!state.selectedLineIndexes.contains(index)) {
                    continue;
                }

                if (!text.empty()) {
                    text.push_back('\n');
                }
                text += view.lines[index];
            }

            if (!text.empty()) {
                ImGui::SetClipboardText(text.c_str());
            }
        }

        void CopyVisibleLines(const LogSnapshot& view)
        {
            if (view.lines.empty()) {
                return;
            }

            std::string text;
            for (const auto& line : view.lines) {
                if (!text.empty()) {
                    text.push_back('\n');
                }
                text += line;
            }

            ImGui::SetClipboardText(text.c_str());
        }

        void DrawLogControls(LogViewerState& state, const LogSnapshot& view, LogRequests& requests, const LogViewerTab::LocalizeFn& localize)
        {
            const auto comboLabel = localize("Logs", "sLogFile", "Log File");
            const std::string preview = view.selectedFile.empty() ? localize("Logs", "sNoLogSelected", "No file") : view.selectedFile;
            const auto& style = ImGui::GetStyle();

            auto wrappedButton = [&](const char* label, bool startOfRow = false) -> bool {
                const float buttonWidth = ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
                if (!startOfRow) {
                    const float needed = style.ItemSpacing.x + buttonWidth;
                    if (ImGui::GetContentRegionAvail().x >= needed) {
                        ImGui::SameLine();
                    }
                }
                return ImGui::Button(label);
            };

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            if (ImGui::BeginCombo(comboLabel, preview.c_str())) {
                for (int i = 0; i < static_cast<int>(view.files.size()); ++i) {
                    const bool isSelected = view.files[i] == view.selectedFile;
                    const auto name = view.files[i];
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        requests.selectFile = name;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const char* refreshLabel = localize("Logs", "sRefreshFiles", "Refresh Files");
            if (wrappedButton(refreshLabel)) {
                requests.refresh = true;
            }

            ImGui::Spacing();

            const char* openFileLabel = localize("Logs", "sOpenFile", "Open File");
            if (ImGui::Button(openFileLabel) && !view.selectedFile.empty()) {
                requests.openFile = true;
            }

            const char* openFolderLabel = localize("Logs", "sOpenFolder", "Open Folder");
            if (wrappedButton(openFolderLabel)) {
                requests.openFolder = true;
            }

            const char* copySelectedLabel = localize("Logs", "sCopySelected", "Copy Selected");
            if (wrappedButton(copySelectedLabel)) {
                CopySelectedLines(state, view);
            }

            const char* copyVisibleLabel = localize("Logs", "sCopyVisible", "Copy Visible");
            if (wrappedButton(copyVisibleLabel)) {
                CopyVisibleLines(view);
            }

            const char* exportLabel = localize("Logs", "sExportFile", "Export File");
            if (wrappedButton(exportLabel)) {
                requests.exportFile = true;
            }

            const char* clearSelLabel = localize("Logs", "sClearSelection", "Clear Selection");
            if (wrappedButton(clearSelLabel)) {
                state.selectedLineIndexes.clear();
            }

            ImGui::Spacing();

            const char* autoScrollLabel = localize("Logs", "sAutoScroll", "Auto Scroll");
            ImGui::Checkbox(autoScrollLabel, &state.autoScroll);
        }

        void DrawLogLines(LogViewerState& state, const LogSnapshot& view, const LogViewerTab::LocalizeFn& localize)
        {
            if (!ImGui::BeginChild("LogLines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
                ImGui::EndChild();
                return;
            }

            for (std::size_t i = 0; i < view.lines.size(); ++i) {
                const bool selected = state.selectedLineIndexes.contains(i);
                char label[32]{};
                std::snprintf(label, sizeof(label), "%06zu", i + 1);

                ImGui::PushID(static_cast<int>(i));
                if (ImGui::Selectable(label, selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    const bool ctrlDown = ImGui::GetIO().KeyCtrl;
                    const bool shiftDown = ImGui::GetIO().KeyShift;

                    if (shiftDown && state.hasLastClicked) {
                        const auto rangeStart = (std::min)(state.lastClickedIndex, i);
                        const auto rangeEnd = (std::max)(state.lastClickedIndex, i);
                        if (!ctrlDown) {
                            state.selectedLineIndexes.clear();
                        }
                        for (std::size_t index = rangeStart; index <= rangeEnd; ++index) {
                            state.selectedLineIndexes.insert(index);
                        }
                    } else if (ctrlDown) {
                        if (selected) {
                            state.selectedLineIndexes.erase(i);
                        } else {
                            state.selectedLineIndexes.insert(i);
                        }
                    } else {
                        state.selectedLineIndexes.clear();
                        state.selectedLineIndexes.insert(i);
                    }

                    state.lastClickedIndex = i;
                    state.hasLastClicked = true;
                }

                if (ImGui::BeginPopupContextItem("LogLineContext")) {
                    if (!state.selectedLineIndexes.contains(i)) {
                        state.selectedLineIndexes.clear();
                        state.selectedLineIndexes.insert(i);
                        state.lastClickedIndex = i;
                        state.hasLastClicked = true;
                    }

                    if (ImGui::MenuItem(localize("Logs", "sCopySelected", "Copy Selected"))) {
                        CopySelectedLines(state, view);
                    }
                    if (ImGui::MenuItem(localize("Logs", "sCopyLine", "Copy Line"))) {
                        ImGui::SetClipboardText(view.lines[i].c_str());
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                ImGui::TextUnformatted(view.lines[i].c_str());
                ImGui::PopID();
            }

            if (state.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 16.0f) {
                ImGui::SetScrollHereY(1.0f);
            }

            if (view.lines.empty()) {
                ImGui::TextDisabled("%s", localize("Logs", "sNoLines", "No log lines available."));
            }

            ImGui::EndChild();
        }
    }

    void LogViewerTab::Draw(LogViewerState& state, const LogSnapshot& view, LogRequests& requests, const LocalizeFn& localize)
    {
        if (state.selectionRevision != view.selectionRevision) {
            state.selectedLineIndexes.clear();
            state.hasLastClicked = false;
            state.selectionRevision = view.selectionRevision;
        }
        DrawLogControls(state, view, requests, localize);
        DrawLogLines(state, view, localize);
    }
}
