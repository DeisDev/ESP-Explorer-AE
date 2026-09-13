#pragma once
#include <cstdint>

namespace ESPExplorerAE
{
    enum class RelationshipKind { WeaponAmmo, RecipeInput, RecipeOutput };
    struct RecordRelationship
    {
        RelationshipKind kind{};
        std::uint32_t target{};
        std::uint32_t quantity{};
    };
    struct IncomingRelationship
    {
        RelationshipKind kind{};
        std::uint32_t source{};
        std::uint32_t quantity{};
    };
}
