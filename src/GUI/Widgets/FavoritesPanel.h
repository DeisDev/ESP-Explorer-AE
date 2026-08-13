#pragma once

#include "Core/Favorites.h"
#include <functional>

namespace ESPExplorerAE
{
    struct FavoritesPanelState
    {
        std::uint64_t generation{};
        std::uint64_t session{};
        std::vector<std::string> tokens;
        std::unordered_set<std::size_t> selected;
        bool stale{};
    };
    struct FavoritesPanelRequests
    {
        FavoriteReview review;
        std::vector<std::size_t> accepted;
    };
    class FavoritesPanel
    {
    public:
        using Localize = std::function<const char*(std::string_view, std::string_view, const char*)>;
        static void Draw(FavoritesPanelState& state, const FavoriteReview& review, const Localize& localize, FavoritesPanelRequests& requests);
    };
}
