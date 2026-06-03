#pragma once

#include "Core/FavoriteIdentity.h"
#include "Core/FavoriteDocument.h"
#include <span>
#include <unordered_set>

namespace ESPExplorerAE
{
    struct FavoriteReviewEntry
    {
        std::string token;
        FavoriteTarget target;
        std::string name;
    };

    struct FavoriteReview
    {
        std::uint64_t generation{};
        std::uint64_t session{};
        bool ready{};
        std::vector<FavoriteReviewEntry> legacy;
        std::vector<FavoriteReviewEntry> unresolved;
        std::size_t temporaryCount{};
    };

    // Render-owner model: durable keys are independent of the current projection.
    // The existing browser set is a frame-local edit surface, never serialized.
    class Favorites
    {
    public:
        void Prepare(const FavoriteDocument& document, std::shared_ptr<const CatalogSnapshot> source, std::uint64_t currentSession)
        {
            if (session != currentSession) temporary.clear();
            if (snapshot == source && session == currentSession && saved == document) return;
            snapshot = std::move(source);
            session = currentSession;
            saved = document;
            projected.clear();
            review = { .generation = snapshot ? snapshot->generation : 0, .session = session, .ready = snapshot && snapshot->ready };
            if (!review.ready) return;
            const FavoriteIdentity identity(*snapshot);
            for (const auto& encoded : saved.keys) {
                const auto key = ParseFavoriteKey(encoded);
                const auto target = key ? identity.Resolve(*key) : FavoriteTarget{ FavoriteResolution::InvalidKey };
                if (target) projected.insert(target.formID);
                else review.unresolved.push_back({ encoded, target, {} });
            }
            for (const auto& token : saved.legacy) {
                const auto id = ParseLegacyFavorite(token);
                const auto target = id ? identity.Capture(*id) : FavoriteTarget{ FavoriteResolution::InvalidKey };
                const auto* record = target ? snapshot->Find(target.formID) : nullptr;
                review.legacy.push_back({ token, target, record ? record->name : std::string{} });
            }
            std::erase_if(temporary, [&](auto id) { return !snapshot->Find(id); });
            projected.insert(temporary.begin(), temporary.end());
            review.temporaryCount = temporary.size();
        }

        std::unordered_set<std::uint32_t>& Forms() { return projected; }
        const FavoriteReview& Review() const { return review; }

        bool ApplyEdits(const std::unordered_set<std::uint32_t>& before, FavoriteDocument& document)
        {
            if (!snapshot || !snapshot->ready || saved != document || before == projected) return false;
            const FavoriteIdentity identity(*snapshot);
            auto next = document;
            for (const auto id : before) if (!projected.contains(id)) {
                temporary.erase(id);
                std::erase_if(next.keys, [&](const auto& encoded) {
                    const auto key = ParseFavoriteKey(encoded);
                    const auto target = key ? identity.Resolve(*key) : FavoriteTarget{};
                    return target && target.formID == id;
                });
            }
            for (const auto id : projected) if (!before.contains(id)) {
                const auto target = identity.Capture(id);
                if (target) {
                    const auto encoded = SerializeFavoriteKey(*target.key);
                    if (std::ranges::find(next.keys, encoded) == next.keys.end()) next.keys.push_back(encoded);
                } else if (snapshot->Find(id)) temporary.insert(id);
            }
            std::ranges::sort(next.keys);
            const bool changed = next != document;
            document = std::move(next);
            // Rebuild the projection even when only temporary favorites changed.
            auto source = snapshot;
            snapshot.reset();
            Prepare(document, std::move(source), session);
            return changed;
        }

        bool AcceptLegacy(const FavoriteReview& accepted, std::span<const std::size_t> indices, FavoriteDocument& document)
        {
            if (!snapshot || !snapshot->ready || accepted.generation != snapshot->generation || accepted.session != session || saved != document) return false;
            auto next = document;
            std::unordered_set<std::size_t> selected;
            const FavoriteIdentity identity(*snapshot);
            for (const auto index : indices) {
                if (index >= saved.legacy.size() || index >= accepted.legacy.size() || !selected.insert(index).second) return false;
                const auto& entry = accepted.legacy[index];
                if (saved.legacy[index] != entry.token || !entry.target || !entry.target.key) return false;
                const auto current = identity.Resolve(*entry.target.key);
                const auto legacyID = ParseLegacyFavorite(entry.token);
                if (!current || !legacyID || current.formID != *legacyID || current.formID != entry.target.formID) return false;
                const auto encoded = SerializeFavoriteKey(*current.key);
                if (std::ranges::find(next.keys, encoded) == next.keys.end()) next.keys.push_back(encoded);
            }
            next.legacy.clear();
            for (std::size_t index = 0; index < document.legacy.size(); ++index) if (!selected.contains(index)) next.legacy.push_back(document.legacy[index]);
            if (next == document) return false;
            std::ranges::sort(next.keys);
            document = std::move(next);
            Prepare(document, snapshot, session);
            return true;
        }

    private:
        FavoriteDocument saved;
        std::shared_ptr<const CatalogSnapshot> snapshot;
        std::uint64_t session{};
        std::unordered_set<std::uint32_t> projected;
        std::unordered_set<std::uint32_t> temporary;
        FavoriteReview review;
    };
}
