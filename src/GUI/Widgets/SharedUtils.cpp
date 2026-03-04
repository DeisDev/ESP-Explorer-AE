#include "GUI/Widgets/SharedUtils.h"

#include <algorithm>
#include <cctype>

namespace ESPExplorerAE
{
    bool SharedUtils::ContainsCaseInsensitive(std::string_view text, std::string_view query)
    {
        if (query.empty()) {
            return true;
        }

        std::string textLower(text.begin(), text.end());
        std::string queryLower(query.begin(), query.end());

        std::ranges::transform(textLower, textLower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        std::ranges::transform(queryLower, queryLower.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

        return textLower.find(queryLower) != std::string::npos;
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
