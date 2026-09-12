#pragma once

#include <string>
#include <vector>

namespace ESPExplorerAE
{
    enum class AdvancedFilterField
    {
        Any = 0,
        Name = 1,
        EditorID = 2,
        Plugin = 3,
        Category = 4,
        Keyword = 5
    };

    enum class AdvancedFilterMatch
    {
        Contains = 0,
        Exact = 1,
        Regex = 2
    };

    struct AdvancedFilterRule
    {
        bool enabled{ true };
        AdvancedFilterField field{ AdvancedFilterField::Any };
        AdvancedFilterMatch match{ AdvancedFilterMatch::Contains };
        std::string value{};
        std::vector<std::string> targetPlugins{};

        bool operator==(const AdvancedFilterRule&) const = default;
    };

}
