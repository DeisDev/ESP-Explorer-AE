#pragma once

#include <memory>

namespace RE
{
    class ExtraDataList;
    class TBO_InstanceData;
    class TESObjectWEAP;
}

namespace ESPExplorerAE
{
    struct InventoryExtraDeleter
    {
        void operator()(RE::ExtraDataList* extra) const;
    };

    // Owns one intrusive reference, including while AddInventoryItem acquires
    // its own references. Cleanup also works when a prepared copy is abandoned.
    using OwnedInventoryExtra = std::unique_ptr<RE::ExtraDataList, InventoryExtraDeleter>;

    OwnedInventoryExtra CopyWeaponInstanceExtra(RE::TESObjectWEAP& weapon,
        const RE::ExtraDataList* source, const RE::TBO_InstanceData* instance);
}
