#pragma once

#include "Data/DataManager.h"
#include "GUI/Widgets/ContextMenu.h"

#include <functional>

namespace ESPExplorerAE
{
    struct FormTableConfig
    {
        const char* tableId{ "FormTable" };
        const char* primaryActionLabel{ "Action" };
        const char* secondaryActionLabel{ nullptr };
        const char* quantityActionLabel{ nullptr };
        bool allowFavorites{ false };
        bool disableBulkPrimaryAction{ false };
    };

    class FormTable
    {
    public:
        using PrimaryAction = std::function<void(const FormEntry&)>;
        using BulkPrimaryAction = std::function<void(const std::vector<FormEntry>&)>;
        using BulkSecondaryAction = std::function<void(const std::vector<FormEntry>&)>;
        using QuantityAction = std::function<void(const FormEntry&, int)>;

        static void Draw(
            const std::vector<FormEntry>& entries,
            std::string_view searchText,
            std::string_view pluginFilter,
            const FormTableConfig& config,
            const PrimaryAction& primaryAction,
            const BulkPrimaryAction& bulkPrimaryAction = {},
            const QuantityAction& quantityAction = {},
            std::unordered_set<std::uint32_t>* favorites = nullptr,
            const ContextMenuCallbacks* contextCallbacks = nullptr,
            const BulkSecondaryAction& bulkSecondaryAction = {});
    };
}
