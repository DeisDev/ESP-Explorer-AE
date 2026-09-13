#pragma once

#include <string_view>

namespace ESPExplorerAE
{
    enum class WorkspaceDestination { Explore, Inventory, PlayerWorld, Tools };
    inline WorkspaceDestination DestinationFor(std::string_view page)
    {
        if (page == "Inventory") return WorkspaceDestination::Inventory;
        if (page == "Player" || page == "Player & World") return WorkspaceDestination::PlayerWorld;
        if (page == "Settings" || page == "Logs" || page == "Tools") return WorkspaceDestination::Tools;
        return WorkspaceDestination::Explore;
    }
    inline std::string_view DefaultDestinationPage(WorkspaceDestination destination)
    {
        switch (destination) {
        case WorkspaceDestination::Explore: return "Plugin Browser";
        case WorkspaceDestination::Inventory: return "Inventory";
        case WorkspaceDestination::PlayerWorld: return "Player & World";
        case WorkspaceDestination::Tools: return "Settings";
        }
        return "Plugin Browser";
    }
}
