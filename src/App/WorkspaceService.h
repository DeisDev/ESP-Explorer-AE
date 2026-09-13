#pragma once
#include "Core/Workspace.h"

namespace ESPExplorerAE
{
    class WorkspaceService
    {
    public:
        static void Load();
        static std::shared_ptr<const WorkspaceDocument> Read();
        static bool Commit(WorkspaceDocument document);
        static bool Flush(bool force = false);
        static bool Writable();
        static std::string Error();
    };
}
