#include "GUI/Tabs/LogViewerTab.h"

#include "Logging/Logger.h"

#include <imgui.h>

#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <unordered_set>

namespace ESPExplorerAE
{
    namespace
    {
        struct LogViewerState
        {
            std::vector<std::filesystem::path> files{};
            int selectedFileIndex{ 0 };
            std::filesystem::path selectedFile{};
            std::vector<std::string> lines{};
            std::unordered_set<std::size_t> selectedLineIndexes{};
            std::uintmax_t readOffset{ 0 };
            std::filesystem::file_time_type lastWriteTime{};
            bool autoScroll{ true };
            bool initialized{ false };
            double lastRefreshTime{ 0.0 };
            std::size_t lastClickedIndex{ 0 };
            bool hasLastClicked{ false };
        };

        LogViewerState state{};

        bool IsLogLikeExtension(const std::filesystem::path& path)
        {
            const auto extension = path.extension().string();
            return extension == ".log" || extension == ".txt";
        }

        void RefreshFileList()
        {
            state.files.clear();

            const auto logDir = Logger::GetLogDirectory();
            if (!std::filesystem::exists(logDir) || !std::filesystem::is_directory(logDir)) {
                return;
            }

            for (const auto& entry : std::filesystem::directory_iterator(logDir)) {
                if (!entry.is_regular_file()) {
                    continue;
                }

                const auto path = entry.path();
                if (!IsLogLikeExtension(path)) {
                    continue;
                }

                state.files.push_back(path);
            }

            std::ranges::sort(state.files, [](const auto& left, const auto& right) {
                return left.filename().string() < right.filename().string();
            });

            if (state.files.empty()) {
                state.selectedFile.clear();
                state.selectedFileIndex = 0;
                return;
            }

            if (state.selectedFile.empty()) {
                const auto mainLogPath = Logger::GetMainLogPath();
                auto match = std::ranges::find(state.files, mainLogPath);
                if (match != state.files.end()) {
                    state.selectedFileIndex = static_cast<int>(std::distance(state.files.begin(), match));
                } else {
                    state.selectedFileIndex = 0;
                }
                state.selectedFile = state.files[state.selectedFileIndex];
                return;
            }

            auto selected = std::ranges::find(state.files, state.selectedFile);
            if (selected == state.files.end()) {
                state.selectedFileIndex = 0;
                state.selectedFile = state.files.front();
            } else {
                state.selectedFileIndex = static_cast<int>(std::distance(state.files.begin(), selected));
            }
        }

        void ResetTailStateForSelection()
        {
            state.lines.clear();
            state.selectedLineIndexes.clear();
            state.readOffset = 0;
            state.lastWriteTime = {};
        }

        void ReadInitialFileContents(const std::filesystem::path& file)
        {
            ResetTailStateForSelection();

            std::ifstream input(file);
            if (!input.is_open()) {
                return;
            }

            std::string line;
            while (std::getline(input, line)) {
                state.lines.push_back(std::move(line));
            }

            while (state.lines.size() > 4000) {
                state.lines.erase(state.lines.begin());
            }

            if (std::filesystem::exists(file)) {
                state.readOffset = std::filesystem::file_size(file);
                state.lastWriteTime = std::filesystem::last_write_time(file);
            }
        }

        void TailSelectedFile()
        {
            if (state.selectedFile.empty() || !std::filesystem::exists(state.selectedFile)) {
                return;
            }

            const auto mainLogPath = Logger::GetMainLogPath();
            if (state.selectedFile == mainLogPath) {
                state.lines = Logger::GetRecentLines();
                return;
            }

            const auto writeTime = std::filesystem::last_write_time(state.selectedFile);
            const auto fileSize = std::filesystem::file_size(state.selectedFile);
            if (fileSize < state.readOffset) {
                ReadInitialFileContents(state.selectedFile);
                return;
            }

            if (writeTime == state.lastWriteTime && fileSize == state.readOffset) {
                return;
            }

            std::ifstream input(state.selectedFile, std::ios::binary);
            if (!input.is_open()) {
                return;
            }

            input.seekg(static_cast<std::streamoff>(state.readOffset), std::ios::beg);

            std::string line;
            while (std::getline(input, line)) {
                state.lines.push_back(std::move(line));
            }

            while (state.lines.size() > 4000) {
                state.lines.erase(state.lines.begin());
            }

            state.readOffset = fileSize;
            state.lastWriteTime = writeTime;
        }

        void CopySelectedLines()
        {
            if (state.selectedLineIndexes.empty()) {
                return;
            }

            std::string text;
            for (std::size_t index = 0; index < state.lines.size(); ++index) {
                if (!state.selectedLineIndexes.contains(index)) {
                    continue;
                }

                if (!text.empty()) {
                    text.push_back('\n');
                }
                text += state.lines[index];
            }

            if (!text.empty()) {
                ImGui::SetClipboardText(text.c_str());
            }
        }

        void CopyVisibleLines()
        {
            if (state.lines.empty()) {
                return;
            }

            std::string text;
            for (const auto& line : state.lines) {
                if (!text.empty()) {
                    text.push_back('\n');
                }
                text += line;
            }

            ImGui::SetClipboardText(text.c_str());
        }

        void ExportSelectedFile()
        {
            if (state.selectedFile.empty() || !std::filesystem::exists(state.selectedFile)) {
                return;
            }

            const auto now = std::chrono::system_clock::now();
            const auto nowTime = std::chrono::system_clock::to_time_t(now);

            std::tm localTime{};
            localtime_s(&localTime, &nowTime);

            char timestamp[32]{};
            std::strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", &localTime);

            const auto sourceName = state.selectedFile.stem().string();
            const auto sourceExt = state.selectedFile.extension().string();
            const auto suggestedPath = (Logger::GetLogDirectory() / (sourceName + "-" + timestamp + sourceExt)).string();

            std::array<char, MAX_PATH> filePathBuffer{};
            std::snprintf(filePathBuffer.data(), filePathBuffer.size(), "%s", suggestedPath.c_str());

            OPENFILENAMEA saveDialog{};
            saveDialog.lStructSize = sizeof(saveDialog);
            saveDialog.lpstrFile = filePathBuffer.data();
            saveDialog.nMaxFile = static_cast<DWORD>(filePathBuffer.size());
            saveDialog.lpstrFilter = "Log Files (*.log;*.txt)\0*.log;*.txt\0All Files (*.*)\0*.*\0";
            saveDialog.nFilterIndex = 1;
            saveDialog.lpstrDefExt = sourceExt.empty() ? "log" : sourceExt.c_str() + 1;
            saveDialog.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

            int showCursorCalls = 0;
            for (;;) {
                ++showCursorCalls;
                const int cursorState = ShowCursor(TRUE);
                if (cursorState >= 0 || showCursorCalls >= 32) {
                    break;
                }
            }

            const bool pickedPath = GetSaveFileNameA(&saveDialog) == TRUE;

            for (int i = 0; i < showCursorCalls; ++i) {
                ShowCursor(FALSE);
            }

            if (!pickedPath) {
                return;
            }

            const std::filesystem::path exportPath{ filePathBuffer.data() };
            if (state.selectedFile.lexically_normal() == exportPath.lexically_normal()) {
                return;
            }

            std::error_code copyError;
            std::filesystem::copy_file(state.selectedFile, exportPath, std::filesystem::copy_options::overwrite_existing, copyError);
            if (copyError) {
                return;
            }

            const std::string args = std::string("/select,\"") + exportPath.string() + "\"";
            ShellExecuteA(nullptr, "open", "explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        }

        void DrawLogControls(const LogViewerTab::LocalizeFn& localize)
        {
            const auto comboLabel = localize("Logs", "sLogFile", "Log File");
            const auto preview = state.selectedFile.empty() ? localize("Logs", "sNoLogSelected", "No file") : state.selectedFile.filename().string().c_str();
            const auto& style = ImGui::GetStyle();

            auto buttonWidth = [&](const char* text) {
                return ImGui::CalcTextSize(text).x + style.FramePadding.x * 2.0f;
            };

            auto wrappedSameLineFor = [&](float width) {
                if (ImGui::GetCursorPosX() > 0.0f && ImGui::GetContentRegionAvail().x < width) {
                    return;
                }
                ImGui::SameLine();
            };

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.4f);
            if (ImGui::BeginCombo(comboLabel, preview)) {
                for (int i = 0; i < static_cast<int>(state.files.size()); ++i) {
                    const bool isSelected = i == state.selectedFileIndex;
                    const auto name = state.files[i].filename().string();
                    if (ImGui::Selectable(name.c_str(), isSelected)) {
                        state.selectedFileIndex = i;
                        state.selectedFile = state.files[i];
                        ReadInitialFileContents(state.selectedFile);
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            const char* refreshLabel = localize("Logs", "sRefreshFiles", "Refresh Files");
            wrappedSameLineFor(buttonWidth(refreshLabel));
            if (ImGui::Button(refreshLabel)) {
                RefreshFileList();
                if (!state.selectedFile.empty()) {
                    ReadInitialFileContents(state.selectedFile);
                }
            }

            ImGui::NewLine();

            const char* openFileLabel = localize("Logs", "sOpenFile", "Open File");
            if (ImGui::Button(openFileLabel) && !state.selectedFile.empty()) {
                const auto target = state.selectedFile.string();
                ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }

            const char* openFolderLabel = localize("Logs", "sOpenFolder", "Open Folder");
            wrappedSameLineFor(buttonWidth(openFolderLabel));
            if (ImGui::Button(openFolderLabel)) {
                const auto folder = Logger::GetLogDirectory().string();
                ShellExecuteA(nullptr, "open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }

            ImGui::NewLine();

            const char* copySelectedLabel = localize("Logs", "sCopySelected", "Copy Selected");
            if (ImGui::Button(copySelectedLabel)) {
                CopySelectedLines();
            }

            const char* copyVisibleLabel = localize("Logs", "sCopyVisible", "Copy Visible");
            wrappedSameLineFor(buttonWidth(copyVisibleLabel));
            if (ImGui::Button(copyVisibleLabel)) {
                CopyVisibleLines();
            }

            const char* exportLabel = localize("Logs", "sExportFile", "Export File");
            wrappedSameLineFor(buttonWidth(exportLabel));
            if (ImGui::Button(exportLabel)) {
                ExportSelectedFile();
            }

            const char* clearSelLabel = localize("Logs", "sClearSelection", "Clear Selection");
            wrappedSameLineFor(buttonWidth(clearSelLabel));
            if (ImGui::Button(clearSelLabel)) {
                state.selectedLineIndexes.clear();
            }

            ImGui::NewLine();

            const char* autoScrollLabel = localize("Logs", "sAutoScroll", "Auto Scroll");
            ImGui::Checkbox(autoScrollLabel, &state.autoScroll);
        }

        void DrawLogLines(const LogViewerTab::LocalizeFn& localize)
        {
            if (!ImGui::BeginChild("LogLines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
                return;
            }

            for (std::size_t i = 0; i < state.lines.size(); ++i) {
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
                        CopySelectedLines();
                    }
                    if (ImGui::MenuItem(localize("Logs", "sCopyLine", "Copy Line"))) {
                        ImGui::SetClipboardText(state.lines[i].c_str());
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                ImGui::TextUnformatted(state.lines[i].c_str());
                ImGui::PopID();
            }

            if (state.autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 16.0f) {
                ImGui::SetScrollHereY(1.0f);
            }

            if (state.lines.empty()) {
                ImGui::TextDisabled("%s", localize("Logs", "sNoLines", "No log lines available."));
            }

            ImGui::EndChild();
        }
    }

    void LogViewerTab::Draw(const LocalizeFn& localize)
    {
        if (!state.initialized) {
            state.initialized = true;
            RefreshFileList();
            if (!state.selectedFile.empty()) {
                ReadInitialFileContents(state.selectedFile);
            }
        }

        const double now = ImGui::GetTime();
        if (now - state.lastRefreshTime >= 0.35) {
            state.lastRefreshTime = now;
            TailSelectedFile();
        }

        DrawLogControls(localize);
        DrawLogLines(localize);
    }
}
