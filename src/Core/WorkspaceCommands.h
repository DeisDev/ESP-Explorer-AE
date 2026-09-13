#pragma once
#include "Core/UnicodeSearch.h"
#include <vector>

namespace ESPExplorerAE
{
    enum class WorkspaceCommandKind { Destination, SavedView, Inspect, Basket, AddSelection, Collections, History, PinA, CompareB, Reference };
    struct WorkspaceCommand
    {
        WorkspaceCommandKind kind{};
        std::string label;
        std::string binding;
        std::string argument;
        bool enabled{true};
        std::string explanation;
    };
    inline std::vector<std::size_t> MatchWorkspaceCommands(const std::vector<WorkspaceCommand>& commands, std::string_view search)
    {
        std::vector<std::size_t> matches;
        for (std::size_t i = 0; i < commands.size(); ++i)
            if (SearchContains(commands[i].label, search) || SearchContains(commands[i].binding, search) || SearchContains(commands[i].argument, search)) matches.push_back(i);
        return matches;
    }
}
