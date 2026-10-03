#pragma once
#include "Config/WorkspaceIni.h"
#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    struct WorkspaceViewState
    {
        bool open{};
        bool focusPending{};
        bool failed{};
        bool storageFailed{};
        std::array<char, 129> name{};
        std::array<char, 129> rename{};
        std::array<char, 4097> note{};
        std::string selectedCollection;
        std::vector<std::uint32_t> collect;
        bool collectRequested{};
        std::string pendingDelete;
        bool deleteRequested{};
        std::optional<WorkspaceReadResult> imported;
        bool importRequested{};
        std::size_t noteIndex{static_cast<std::size_t>(-1)};
    };
    struct WorkspaceViewRequests
    {
        std::optional<WorkspaceDocument> update;
        std::optional<WorkspaceLocation> restore;
        std::vector<std::uint32_t> inspect;
        std::optional<std::pair<std::string, std::string>> renamedCollection;
        std::optional<std::string> deletedCollection;
        std::optional<std::string> searchCollection;
        bool manage{};
    };
    void DrawWorkspaceSources(WorkspaceViewState& state, const WorkspaceDocument& document, RecordScope& scope,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests);
    // Draws the popup opened with ImGui::OpenPopup("SavedViewsMenu") in the same window.
    void DrawSavedViewsMenu(WorkspaceViewState& state, const WorkspaceDocument& document, const WorkspaceLocation& current,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests);
    void DrawCollectionsWindow(WorkspaceViewState& state, const WorkspaceDocument& document,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests);
    void DrawAddToCollectionPopup(WorkspaceViewState& state, const WorkspaceDocument& document,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests);
}
