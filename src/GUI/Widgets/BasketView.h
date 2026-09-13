#pragma once
#include "Core/ItemKit.h"
#include "GUI/BrowserState.h"

namespace ESPExplorerAE
{
    struct BasketViewState
    {
        bool focusPending{};
        bool submitted{};
        bool saveFailed{};
        bool full{};
        std::array<char, 129> name{};
        std::optional<KitReview> reviewed;
        ActionAdmission admission{ActionAdmission::Accepted};
    };
    struct BasketRequests
    {
        std::optional<KitReview> execute;
        std::optional<ItemKit> save;
        std::vector<std::uint32_t> inspect;
        bool history{};
    };
    void DrawBasketWindow(bool& open, BasketViewState& state, ItemKit& kit, const BrowserView& view, std::size_t pending, BasketRequests& requests);
}
