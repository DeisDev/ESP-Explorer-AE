#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/Actions.h"
#include "Core/RecordSelection.h"
#include "Core/RecordDetails.h"
#include "GUI/BrowserState.h"
#include <array>

#include <functional>

namespace ESPExplorerAE
{
    struct InventoryEntry
    {
        std::uint32_t formID{ 0 };
        std::string name;
        std::string category;
        std::string sourcePlugin;
        std::uint64_t count{ 0 };
        float weight{ 0.0f };
        std::int32_t value{ 0 };
        double totalWeight{};
        std::int64_t totalValue{};
        bool isEquipped{ false };
        bool isFavorited{ false };
        bool isLegendary{ false };
        bool isQuestItem{ false };
        std::uint32_t modCount{ 0 };
        std::uint16_t damage{ 0 };
        std::uint16_t armorRating{ 0 };
        std::uint64_t groupID{};
        std::string editorID;
        std::shared_ptr<const InventorySnapshot> source;
        std::vector<InventoryIndex> stacks;
        InventoryIndex representative{};
    };

    enum class InventoryCategoryTab { All, Weapons, Armor, Ammo, Aid, Misc, Keys, Notes, Components, Junk };

    struct InventoryQuickState
    {
        int currentAmmo{ 200 };
        int allAmmo{ 100 };
        int perkPoints{ 1 };
        int level{ 1 };
        float gameHour{ 12.0f };
    };

    struct InventoryTabState
    {
        std::shared_ptr<const InventorySnapshot> snapshot;
        std::uint64_t catalogGeneration{};
        std::vector<InventoryEntry> cachedInventory;
        std::array<char, 256> inventorySearchBuffer{};
        std::string inventorySearch;
        InventoryCategoryTab activeCategory{ InventoryCategoryTab::All };
        bool showEquippedOnly{};
        bool showFavoritesOnly{};
        bool showLegendaryOnly{};
        bool showQuestOnly{};
        bool resetCategory{};
        bool focusPending{};
        InventoryQuickState quick;
        bool selectionChanged{};
        std::uint64_t detailsGroup{};
        std::uint64_t detailsToken{};
        ActionAdmission detailsAdmission{ ActionAdmission::Accepted };
        InventoryRejection detailsRejection{ InventoryRejection::None };
        OrderedSelection<std::uint64_t> selection;
        std::unordered_map<std::uint64_t, std::uint64_t> instanceChoices;
        std::unordered_map<std::uint64_t, int> desiredCounts;
        ActionAdmission admission{ ActionAdmission::Accepted };
        InventoryRejection rejection{ InventoryRejection::None };
    };

    struct InventoryTabView
    {
        std::function<const char*(std::string_view, std::string_view, const char*)> localize;
        std::shared_ptr<const CatalogSnapshot> catalog;
        std::shared_ptr<const InventorySnapshot> inventory;
        std::shared_ptr<const RecordDetails> details;
        std::uint64_t session{};
        bool gameplayReady{};
        bool godMode{};
        bool advancedDetails{};
    };

    struct InventoryTabRequests
    {
        struct Confirmation { std::string title; std::string message; std::vector<ActionRequest> actions; };
        BrowserRequests records;
        std::vector<Confirmation> confirmations;
        std::optional<std::uint32_t> inspect;
        std::optional<DetailKey> details;
        bool refresh{};
    };

    class InventoryTab
    {
    public:
        static void Draw(InventoryTabState& state, const InventoryTabView& view, InventoryTabRequests& requests);
        static void ResetFilters(InventoryTabState& state);
        static void ResetState(InventoryTabState& state);
    };
}
