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
        std::array<char, 4097> note{};
        std::string selectedCollection;
        std::vector<std::uint32_t> collect;
        std::optional<WorkspaceReadResult> imported;
        std::size_t noteIndex{static_cast<std::size_t>(-1)};
    };
    struct WorkspaceViewRequests
    {
        bool changed{};
        std::optional<WorkspaceLocation> restore;
        std::vector<std::uint32_t> inspect;
        std::optional<ItemKit> loadKit;
    };
    void DrawWorkspaceSources(WorkspaceViewState& state, const WorkspaceDocument& document, RecordScope& scope,
        const BrowserView& view, WorkspaceViewRequests& requests);
    void DrawWorkspaceWindow(WorkspaceViewState& state, WorkspaceDocument& document, const WorkspaceLocation& current,
        const BrowserView& view, bool writable, WorkspaceViewRequests& requests);
}
