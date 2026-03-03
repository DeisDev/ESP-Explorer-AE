#pragma once

#include "Data/DataManager.h"

#include <functional>

namespace ESPExplorerAE
{
    struct FormTableConfig
    {
        const char* tableId{ "FormTable" };
        const char* primaryActionLabel{ "Action" };
        const char* quantityActionLabel{ nullptr };
        bool allowFavorites{ false };
    };

    class FormTable
    {
    public:
        using PrimaryAction = std::function<void(const FormEntry&)>;
        using QuantityAction = std::function<void(const FormEntry&, int)>;

        static void Draw(
            const std::vector<FormEntry>& entries,
            std::string_view searchText,
            std::string_view pluginFilter,
            const FormTableConfig& config,
            const PrimaryAction& primaryAction,
            const QuantityAction& quantityAction = {},
            std::unordered_set<std::uint32_t>* favorites = nullptr,
            bool* caseSensitiveOverride = nullptr);
    };
}
