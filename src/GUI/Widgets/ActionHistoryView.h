#pragma once

#include "GUI/Widgets/ActionHistory.h"
#include <imgui.h>
#include <array>

namespace ESPExplorerAE
{
    struct ActionHistoryViewState
    {
        bool open{};
        bool focusPending{};
        bool issuesOnly{};
        std::array<char, 256> search{};
        std::optional<std::uint32_t> inspect;
    };

    // Returns the current position for the caller's existing INI persistence.
    ImVec2 DrawActionHistoryWindow(ActionHistoryViewState& state, std::span<const ActionReceipt> receipts,
        const ActionHistoryLocalize& localize, ImVec2 initialPosition);
}
