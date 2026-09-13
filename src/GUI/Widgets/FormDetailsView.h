#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/RecordDetails.h"

#include <functional>

namespace ESPExplorerAE
{
    struct FormDetailsViewContext
    {
        std::function<const char*(std::string_view, std::string_view, const char*)> localize;
        bool showAdvancedDetailsView{ false };
        const CatalogSnapshot* catalog{};
        std::shared_ptr<const RecordDetails> details;
        std::function<void(std::uint32_t)> open;
        std::function<void(std::uint32_t)> pin;
        std::function<void(std::uint32_t)> collect;
        bool canPin{ true };
    };

    class FormDetailsView
    {
    public:
        static void Draw(const FormEntry& selectedRecord, const FormDetailsViewContext& context);
    };
}
