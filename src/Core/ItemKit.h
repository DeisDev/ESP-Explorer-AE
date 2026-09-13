#pragma once
#include "Core/Workspace.h"
#include "Core/RecordActions.h"
#include <map>

namespace ESPExplorerAE
{
    enum class KitIssue { None, Empty, Unresolved, Ineligible, Quantity, Ammo, Capacity, Unavailable, Stale };
    struct KitReviewRow
    {
        std::uint32_t formID{};
        std::string name;
        std::uint32_t quantity{};
        std::uint32_t ammoID{};
        std::uint32_t ammoQuantity{};
        KitIssue issue{};
    };
    struct KitReview
    {
        std::uint64_t session{};
        std::uint64_t generation{};
        std::vector<KitReviewRow> rows;
        std::vector<ActionRequest> actions;
        std::uint64_t itemTotal{};
        std::uint64_t ammoTotal{};
        KitIssue issue{};
        explicit operator bool() const { return issue == KitIssue::None && !actions.empty(); }
    };
    inline KitReview ReviewItemKit(const ItemKit& kit, const CatalogSnapshot& catalog, std::uint64_t session, bool ready, std::size_t pending, bool substituteComponents = true)
    {
        KitReview review{session, catalog.generation};
        if (kit.entries.empty()) { review.issue = KitIssue::Empty; return review; }
        if (kit.entries.size() > ActionQueue::Capacity) { review.issue = KitIssue::Capacity; return review; }
        FavoriteIdentity identity(catalog);
        std::map<std::uint32_t, std::uint64_t> totals;
        for (const auto& entry : kit.entries) {
            KitReviewRow row{0, entry.record.name, entry.quantity};
            const auto target = ResolveWorkspaceRecord(identity, entry.record, session);
            const auto* record = target ? catalog.Find(target.formID) : nullptr;
            if (record && record->category == "CMPO" && substituteComponents) record = catalog.Find(record->componentItemID);
            if (!record) row.issue = KitIssue::Unresolved;
            else {
                row.formID = record->formID; row.name = record->name;
                if (record->isDeleted || record->category == "CMPO" || !SupportsRecordAction(record->category, ActionKind::Give)) row.issue = KitIssue::Ineligible;
                else if (!entry.quantity || entry.quantity > ActionQueue::MaxQuantity || entry.ammoQuantity > ActionQueue::MaxQuantity) row.issue = KitIssue::Quantity;
                else if (entry.includeAmmo && entry.ammoQuantity) {
                    const auto* ammo = catalog.Find(record->weaponAmmoID);
                    if (record->category != "WEAP" || !ammo || ammo->category != "AMMO" || ammo->isDeleted) row.issue = KitIssue::Ammo;
                    else { row.ammoID = ammo->formID; row.ammoQuantity = entry.ammoQuantity; }
                }
            }
            if (row.issue != KitIssue::None) review.issue = row.issue;
            else {
                totals[row.formID] += row.quantity;
                review.itemTotal += row.quantity;
                if (row.ammoQuantity) { totals[row.ammoID] += row.ammoQuantity; review.ammoTotal += row.ammoQuantity; }
            }
            review.rows.push_back(std::move(row));
        }
        for (const auto& [id, quantity] : totals) {
            if (quantity > ActionQueue::MaxQuantity) { review.issue = KitIssue::Quantity; break; }
            review.actions.push_back({.kind = ActionKind::Give, .formID = id, .count = static_cast<std::uint32_t>(quantity), .session = session});
        }
        if (review.actions.size() > ActionQueue::Capacity || pending > ActionQueue::Capacity || review.actions.size() > ActionQueue::Capacity - pending) review.issue = KitIssue::Capacity;
        if (!ready || !session) review.issue = KitIssue::Unavailable;
        if (review.issue != KitIssue::None) review.actions.clear();
        return review;
    }
    inline bool SameKitReview(const KitReview& reviewed, const KitReview& current)
    {
        if (!reviewed || !current || reviewed.session != current.session || reviewed.generation != current.generation || reviewed.actions.size() != current.actions.size()) return false;
        for (std::size_t i = 0; i < current.actions.size(); ++i) if (reviewed.actions[i].formID != current.actions[i].formID || reviewed.actions[i].count != current.actions[i].count) return false;
        return true;
    }
}
