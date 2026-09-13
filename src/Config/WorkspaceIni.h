#pragma once
#include "Core/Workspace.h"

namespace ESPExplorerAE
{
    struct WorkspaceReadResult
    {
        WorkspaceDocument document;
        std::string error;
        bool newerSchema{};
        explicit operator bool() const { return error.empty(); }
    };
    WorkspaceReadResult ReadWorkspaceIni(std::string_view bytes);
    bool WriteWorkspaceIni(const WorkspaceDocument& document, std::string& bytes);
}
