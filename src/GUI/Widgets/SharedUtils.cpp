#include "GUI/Widgets/SharedUtils.h"

#include <algorithm>
#include <cctype>

namespace ESPExplorerAE
{
    namespace
    {
        char FoldCase(unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        }
    }

    bool SharedUtils::ContainsCaseInsensitive(std::string_view text, std::string_view query)
    {
        if (query.empty()) {
            return true;
        }

        const auto match = std::search(text.begin(), text.end(), query.begin(), query.end(), [](char left, char right) {
            return FoldCase(static_cast<unsigned char>(left)) == FoldCase(static_cast<unsigned char>(right));
        });

        return match != text.end();
    }

    bool SharedUtils::ContainsByMode(std::string_view text, std::string_view query, bool caseSensitive)
    {
        if (query.empty()) {
            return true;
        }

        if (caseSensitive) {
            return text.find(query) != std::string::npos;
        }

        return ContainsCaseInsensitive(text, query);
    }

    std::string SharedUtils::BuildParenthesizedList(const std::vector<std::string>& values)
    {
        std::string text{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                text += ", ";
            }
            text += "(";
            text += values[i];
            text += ")";
        }
        return text;
    }
}
