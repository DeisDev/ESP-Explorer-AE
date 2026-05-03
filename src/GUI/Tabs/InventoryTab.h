#pragma once

#include "Data/DataManager.h"

#include <functional>

namespace ESPExplorerAE
{
    struct InventoryEntry
    {
        std::uint32_t formID{ 0 };
        std::string name;
        std::string category;
        std::string sourcePlugin;
        std::uint32_t count{ 0 };
        float weight{ 0.0f };
        std::int32_t value{ 0 };
        bool isEquipped{ false };
        bool isFavorited{ false };
        bool isLegendary{ false };
        bool isQuestItem{ false };
        std::uint32_t modCount{ 0 };
        std::uint16_t damage{ 0 };
        std::uint16_t armorRating{ 0 };
        std::uint32_t stackID{ 0 };
    };

    struct InventoryTabContext
    {
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        LocalizeFn localize;
        bool& playerGodModeEnabled;
        bool& playerNoClipEnabled;
        int& playerCurrentWeaponAmmoAmount;
        int& playerAllAmmoAmount;
        int& playerPerkPointsAmount;
        int& playerLevelAmount;
        float& playerTimeOfDay;
        const FormCache& cache;
        bool* searchFocusPending{ nullptr };
        std::function<void(const FormEntry&)> openItemGrantPopup;
        std::function<void(std::uint32_t)> inspectFormInPluginBrowser;
    };

    class InventoryTab
    {
    public:
        static void Draw(InventoryTabContext& context);
        static void ResetState();
    };
}