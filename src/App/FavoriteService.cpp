#include "App/FavoriteService.h"
#include "App/ActionService.h"
#include "Config/Config.h"
#include "App/CatalogService.h"

namespace ESPExplorerAE
{
    namespace { Favorites favorites; }

    void FavoriteService::Prepare(std::shared_ptr<const CatalogSnapshot> catalog, std::uint64_t session)
    {
        favorites.Prepare(Config::Get().favorites, std::move(catalog), session);
    }
    std::unordered_set<std::uint32_t>& FavoriteService::Forms() { return favorites.Forms(); }
    const FavoriteReview& FavoriteService::Review() { return favorites.Review(); }
    void FavoriteService::CommitEdits(const std::unordered_set<std::uint32_t>& before)
    {
        if (favorites.ApplyEdits(before, Config::GetMutable().favorites)) Config::RequestSave();
    }
    bool FavoriteService::AcceptLegacy(const FavoriteReview& review, std::span<const std::size_t> selected)
    {
        Prepare(CatalogService::Read(), ActionService::Session());
        if (!favorites.AcceptLegacy(review, selected, Config::GetMutable().favorites)) return false;
        Config::RequestSave();
        return true;
    }
    void FavoriteService::Reset() { favorites = Favorites{}; }
}
