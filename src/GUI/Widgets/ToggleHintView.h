#pragma once

#include <functional>
#include <string_view>

namespace ESPExplorerAE::ToggleHintView
{
    void Draw(std::string_view key, float opacity,
        const std::function<const char*(std::string_view, std::string_view, const char*)>& localize);
}
