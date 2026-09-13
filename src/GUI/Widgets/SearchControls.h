#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/SearchQuery.h"
#include <functional>

namespace ESPExplorerAE
{
    void DrawSearchControls(bool& structured, RecordScope& scope, std::string& search, char* buffer, std::size_t size,
        const CatalogSnapshot& catalog, const std::function<const char*(std::string_view, std::string_view, const char*)>& localize);
    void DrawSearchFeedback(bool structured, const RecordScope& scope, std::string_view search,
        const std::function<const char*(std::string_view, std::string_view, const char*)>& localize);
}
