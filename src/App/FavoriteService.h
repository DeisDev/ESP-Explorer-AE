#pragma once

#include "Core/Favorites.h"

namespace ESPExplorerAE
{
    // Called by the render-owned composition root. Persistence stores durable
    // keys; the browser-facing runtime-ID set is only the current projection.
    class FavoriteService
    {
    public:
        static void Prepare(std::shared_ptr<const CatalogSnapshot> catalog, std::uint64_t session);
        static std::unordered_set<std::uint32_t>& Forms();
        static const FavoriteReview& Review();
        static void CommitEdits(const std::unordered_set<std::uint32_t>& before);
        static bool AcceptLegacy(const FavoriteReview& review, std::span<const std::size_t> selected);
        static void Reset();
    };
}
