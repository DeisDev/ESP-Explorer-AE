#pragma once

#include "Data/DataManager.h"

#include <functional>

namespace ESPExplorerAE
{
    struct FormDetailsViewContext
    {
        std::function<const char*(std::string_view, std::string_view, const char*)> localize;
        bool showAdvancedDetailsView{ false };
    };

    class FormDetailsView
    {
    public:
        static void Draw(const FormEntry& selectedRecord, const FormDetailsViewContext& context);
    };
}