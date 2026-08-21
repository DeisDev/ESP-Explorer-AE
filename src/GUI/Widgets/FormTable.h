#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/CopyFormat.h"
#include "Core/QueryIdentity.h"
#include "Core/RecordSelection.h"

#include <functional>
#include <optional>

namespace ESPExplorerAE
{
    struct FormTableConfig
    {
        const char* tableId{ "FormTable" };
        const char* primaryActionLabel{ nullptr };
        const char* secondaryActionLabel{ nullptr };
        const char* quantityActionLabel{ nullptr };
        bool allowFavorites{ false };
        bool disableBulkPrimaryAction{ false };
        bool gameplayActionsAllowed{};
        MultiCopyFormat copyFormat{ MultiCopyFormat::Lines };
    };

    struct FormTableState
    {
        struct Sort { int column{ 1 }; bool ascending{ true }; } sort;
        RecordSelection selection;
        int quantity{ 1 };
        int bulkQuantity{ 1 };
        std::optional<ResultRevision> orderRevision;
        std::vector<std::uint32_t> displayedIDs;
        std::unordered_map<std::uint32_t, std::size_t> displayedPositions;
    };

    struct FormTableActions
    {
        std::function<void(const FormEntry&)> primary;
        std::function<void(const std::vector<FormEntry>&)> bulkPrimary;
        std::function<void(const FormEntry&, int)> quantity;
        std::function<void(const std::vector<FormEntry>&)> bulkSecondary;
        std::function<void(const FormEntry&, bool)> rowContext;
        std::function<bool(const FormEntry&)> canPrimary;
        std::function<void(std::uint32_t)> selected;
    };

    class FormTable
    {
    public:
        static void DrawPrepared(FormTableState& state, const CatalogResult& result, const FormTableConfig& config,
            const FormTableActions& actions, std::unordered_set<std::uint32_t>* favorites = nullptr);
    };
}
