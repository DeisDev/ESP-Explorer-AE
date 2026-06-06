#pragma once

#include <algorithm>
#include <cstdint>
#include <map>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ESPExplorerAE
{
    struct InventoryMod
    {
        std::uint32_t formID{};
        std::uint8_t attachIndex{};
        std::uint8_t rank{};
        bool disabled{};
        std::string slotLabel;
        std::string name;
        friend bool operator==(const InventoryMod&, const InventoryMod&) = default;
    };

    struct InventoryEnchantment
    {
        std::uint32_t formID{};
        std::string name;
        friend bool operator==(const InventoryEnchantment&, const InventoryEnchantment&) = default;
    };

    // Detached values only. Tokens are minted by the game adapter for retained
    // stack objects, never derived from an engine list position or display name.
    struct InventoryStack
    {
        std::uint64_t token{};
        std::uint64_t instanceRevision{};
        std::uint32_t formID{};
        std::string name;
        std::string category;
        std::string sourcePlugin;
        std::uint32_t count{};
        float weight{};
        std::int32_t value{};
        bool isEquipped{};
        bool isFavorited{};
        bool isLegendary{};
        bool isQuestItem{};
        bool isDeleted{};
        bool instanceIdentityKnown{ true };
        std::uint16_t flags{};
        std::uint16_t damage{};
        std::uint16_t armorRating{};
        std::uint32_t uniqueBaseID{};
        std::uint16_t uniqueID{};
        float healthPercent{ -1.0f };
        std::uint32_t legendaryFormID{};
        std::string legendaryName;
        std::uint32_t modCount{};
        std::vector<InventoryMod> mods;
        std::vector<InventoryEnchantment> enchantments;
        friend bool operator==(const InventoryStack&, const InventoryStack&) = default;
    };

    using InventoryIndex = std::size_t;

    struct InventoryGroup
    {
        std::uint64_t id{};
        std::vector<InventoryIndex> stacks;
        InventoryIndex representative{};
        std::uint64_t count{};
        double totalWeight{};
        std::int64_t totalValue{};
        bool anyEquipped{};
        bool anyFavorited{};
        bool anyLegendary{};
        bool anyQuestItem{};
    };

    struct InventorySnapshot
    {
        std::uint64_t session{};
        std::uint64_t generation{};
        bool ready{};
        float carryWeight{};
        std::uint64_t capsCount{};
        std::vector<InventoryStack> stacks;
        std::vector<InventoryGroup> groups;
        std::unordered_map<std::uint64_t, InventoryIndex> byToken;

        const InventoryStack* Find(std::uint64_t token) const
        {
            const auto found = byToken.find(token);
            return found == byToken.end() ? nullptr : &stacks[found->second];
        }

        void BuildGroups()
        {
            byToken.clear();
            groups.clear();
            std::map<std::pair<std::uint32_t, std::string>, std::vector<InventoryIndex>> members;
            for (InventoryIndex index = 0; index < stacks.size(); ++index) {
                const auto& stack = stacks[index];
                if (!stack.token || !stack.formID || !stack.count || !byToken.emplace(stack.token, index).second)
                    throw std::invalid_argument("inventory stacks require unique nonzero identities and positive counts");
                members[{ stack.formID, stack.name }].push_back(index);
            }
            for (auto& [key, indices] : members) {
                std::ranges::sort(indices, {}, [&](InventoryIndex index) { return stacks[index].token; });
                InventoryGroup group{ .id = stacks[indices.front()].token, .stacks = std::move(indices) };
                group.representative = group.stacks.front();
                for (const auto index : group.stacks) {
                    const auto& stack = stacks[index];
                    if (!group.anyEquipped && stack.isEquipped) group.representative = index;
                    group.count += stack.count;
                    group.totalWeight += static_cast<double>(stack.weight) * stack.count;
                    const auto value = static_cast<std::int64_t>(stack.value) * stack.count;
                    if ((value > 0 && group.totalValue > (std::numeric_limits<std::int64_t>::max)() - value) ||
                        (value < 0 && group.totalValue < (std::numeric_limits<std::int64_t>::min)() - value))
                        throw std::overflow_error("inventory group value exceeds its representation");
                    group.totalValue += value;
                    group.anyEquipped |= stack.isEquipped;
                    group.anyFavorited |= stack.isFavorited;
                    group.anyLegendary |= stack.isLegendary;
                    group.anyQuestItem |= stack.isQuestItem;
                }
                groups.push_back(std::move(group));
            }
        }
    };
}
