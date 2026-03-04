#pragma once

#include "Data/DataManager.h"

#include <functional>
#include <vector>

namespace ESPExplorerAE
{
    class ItemGrantPopup
    {
    public:
        using LocalizeFn = std::function<const char*(std::string_view, std::string_view, const char*)>;

        static void Open(const FormEntry& entry);
        static void Open(const std::vector<FormEntry>& entries);
        static void Draw(const LocalizeFn& localize);

    private:
        struct State
        {
            struct ItemState
            {
                FormEntry entry{};
                int quantity{ 1 };
                int ammoQuantity{ 100 };
                std::uint32_t ammoFormID{ 0 };
                bool includeAmmo{ false };
            };

            bool openRequested{ false };
            bool visible{ false };
            std::vector<ItemState> items{};
        };

        static State& GetState();
    };
}
