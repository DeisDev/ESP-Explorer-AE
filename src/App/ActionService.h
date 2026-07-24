#pragma once

#include "Core/Actions.h"
#include <vector>

namespace ESPExplorerAE
{
    class ActionService
    {
    public:
        static bool Initialize();
        static void BeginSession(bool mayBecomeReady);
        static void UpdatePolicy(bool substituteComponents, bool allowMainMenu);
        static bool Submit(ActionRequest request);
        static ActionAdmission SubmitBatch(std::vector<ActionRequest> requests);
        static bool IsReady();
        static bool GodModeEnabled();
        static std::uint64_t Session();
        static std::size_t PendingCount();
        static std::vector<ActionReceipt> Receipts();
        static void Pump();
    };
}
