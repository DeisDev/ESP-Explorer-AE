#pragma once

#include "Core/InventoryActions.h"

namespace ESPExplorerAE::InventoryFeedback
{
    template <class Localize>
    std::string Message(InventoryRejection rejection, const Localize& localize)
    {
        switch (rejection) {
        case InventoryRejection::None: return {};
        case InventoryRejection::Unavailable: return localize("Inventory", "sUnavailable", "Inventory is unavailable.");
        case InventoryRejection::StaleSession: case InventoryRejection::MissingStack: case InventoryRejection::ChangedStack:
            return localize("Inventory", "sSelectionChanged", "Inventory changed. Select the items again.");
        case InventoryRejection::Invalid: return localize("Inventory", "sInvalidTarget", "The selected inventory action cannot be validated.");
        case InventoryRejection::QuestItem: return localize("Inventory", "sQuestRestriction", "Quest items cannot be removed or dropped.");
        case InventoryRejection::AmbiguousInstance: return localize("Inventory", "sInstanceRequired", "Choose an instance to equip, use, or duplicate. Group removal affects all captured stacks.");
        case InventoryRejection::TooLarge: return localize("Inventory", "sReduceSelection", "Reduce the selected stacks or quantity, then try again.");
        }
        return {};
    }
}
