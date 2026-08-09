#pragma once

#include "Core/Actions.h"
#include <imgui.h>

namespace ESPExplorerAE::ActionFeedback
{
    template <class Localize>
    void Draw(ActionAdmission admission, const Localize& localize)
    {
        std::string message;
        switch (admission) {
        case ActionAdmission::Accepted: return;
        case ActionAdmission::Unavailable: message = localize("General", "sBatchUnavailable", "No actions were queued. Gameplay is not ready."); break;
        case ActionAdmission::StaleSession: message = localize("General", "sBatchStale", "No actions were queued. Select records again in the current game session."); break;
        case ActionAdmission::Invalid: message = localize("General", "sBatchInvalid", "No actions were queued. The selection contains an invalid request."); break;
        case ActionAdmission::QueueFull: message = localize("General", "sBatchQueueFull", "No actions were queued. Wait for pending actions to finish, then try again."); break;
        case ActionAdmission::BatchTooLarge:
            message = localize("General", "sBatchTooLarge", "No actions were queued. Select at most {limit} records per batch.");
            if (const auto token = message.find("{limit}"); token != std::string::npos) message.replace(token, 7, std::to_string(ActionQueue::Capacity));
            break;
        }
        ImGui::TextWrapped("%s", message.c_str());
    }
}
