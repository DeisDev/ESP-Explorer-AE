#pragma once
#include "Core/WorkspaceCommands.h"
#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    struct CommandPaletteState
    {
        bool requested{};
        bool focus{};
        std::array<char, 256> buffer{};
        std::string search;
        std::size_t selected{};
    };
    std::optional<WorkspaceCommand> DrawCommandPalette(CommandPaletteState& state, const std::vector<WorkspaceCommand>& commands, const BrowserView& view);
}
