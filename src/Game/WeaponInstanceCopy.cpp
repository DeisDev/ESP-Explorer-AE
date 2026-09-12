#include "Game/WeaponInstanceCopy.h"
#include "pch.h"

#include <RE/B/BGSObjectInstanceExtra.h>
#include <RE/E/ExtraInstanceData.h>

namespace ESPExplorerAE
{
    namespace
    {
        // Missing from the pinned CommonLib wrapper. Verified in Fallout 4
        // 1.11.240: CopyList's UniqueID case calls this at RVA 0x273246;
        // RVA 0x2A18D0 locks the list, unlinks by uint8 type, destroys the node,
        // and returns bool. Use the engine unlinker to maintain the list tail.
        void RemoveCopiedExtra(RE::ExtraDataList& extra, RE::EXTRA_DATA_TYPE type)
        {
            using Function = bool (*)(RE::ExtraDataList*, std::uint8_t);
            static REL::Relocation<Function> remove{ REL::ID{ 2190971 } };
            remove(&extra, static_cast<std::uint8_t>(type));
        }
    }

    void InventoryExtraDeleter::operator()(RE::ExtraDataList* extra) const
    {
        if (!extra || extra->DecRef() != 0) return;
        // CommonLib's ExtraDataList has no destructor for its engine-owned
        // nodes/presence bits. Match AddInventoryItem's release at 1.11.240
        // RVA 0x5038CC: non-deleting destructor (0x272A90), then game heap free.
        using Function = void (*)(RE::ExtraDataList*);
        static REL::Relocation<Function> destroy{ REL::ID{ 2190089 } };
        destroy(extra);
        RE::free(extra);
    }

    OwnedInventoryExtra CopyWeaponInstanceExtra(RE::TESObjectWEAP& weapon,
        const RE::ExtraDataList* source, const RE::TBO_InstanceData* instance)
    {
        if (instance && !RE::fallout_cast<const RE::TESObjectWEAP::InstanceData*>(instance)) return {};
        OwnedInventoryExtra copy{ new RE::ExtraDataList };
        copy->IncRef();
        if (source) copy->CopyList(source);

        // CopyList deep-copies BGSObjectInstanceExtra (including disabled mods,
        // attachment indices and ranks), but shares ExtraInstanceData::data.
        // TESBoundObject::CreateInstanceData(source) dispatches the weapon's
        // engine copy constructor: 1.11.240 RVA 0x478100 -> 0x473AF0. It copies
        // all instance stats and owned arrays, without reapplying/rerolling mods.
        RE::BSTSmartPointer<RE::TBO_InstanceData> copiedInstance{ weapon.CreateInstanceData(instance) };
        if (!copiedInstance || copiedInstance.get() == instance || copiedInstance.get() == weapon.GetBaseInstanceData()) return {};
        if (auto* data = copy->GetByType<RE::ExtraInstanceData>()) {
            data->base = &weapon;
            data->data = std::move(copiedInstance);
        } else {
            copy->AddExtra(new RE::ExtraInstanceData(&weapon, std::move(copiedInstance)));
        }
        // Even an unmodified weapon must suppress AddInventoryItem's template
        // generation. An explicit empty mod list preserves its current stats.
        if (!copy->HasType<RE::BGSObjectInstanceExtra>()) copy->AddExtra(new RE::BGSObjectInstanceExtra);

        // The copy is a new item, not another handle to the original's quest,
        // hotkey, equipped or inventory identity. Keep name/condition/ownership
        // and gameplay extras; stack equipment flags are never copied.
        for (const auto type : { RE::kUniqueID, RE::kFavorite, RE::kCount,
                 RE::kAliasInstanceArray, RE::kFromAlias, RE::kReferenceHandle,
                 RE::kOriginalReference, RE::kItemDropper, RE::kDroppedItemList }) {
            RemoveCopiedExtra(*copy, type);
        }
        return copy;
    }
}
