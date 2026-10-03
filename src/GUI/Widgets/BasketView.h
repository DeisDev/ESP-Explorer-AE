#pragma once
#include "Core/ItemKit.h"
#include "GUI/BrowserState.h"

#include <span>

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
        std::optional<ItemKit> load;
        bool replace{};
        std::optional<std::string> removeKit;
        std::vector<std::uint32_t> inspect;
    };
    void DrawBasketWindow(bool& open, BasketViewState& state, ItemKit& kit, std::span<const ItemKit> savedKits, bool writable,
        const BrowserView& view, std::size_t pending, BasketRequests& requests);
}
