#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ESPExplorerAE
{
    struct FavoriteDocument
    {
        // Encoded keys, including invalid/unrecognized entries, survive saves.
        std::vector<std::string> keys;
        std::vector<std::string> legacy;
        friend bool operator==(const FavoriteDocument&, const FavoriteDocument&) = default;
    };

    inline std::vector<std::string> SplitFavoriteTokens(std::string_view csv)
    {
        std::vector<std::string> result;
        while (!csv.empty()) {
            const auto end = csv.find(',');
            const auto token = csv.substr(0, end);
            if (!token.empty()) result.emplace_back(token);
            if (end == std::string_view::npos) break;
            csv.remove_prefix(end + 1);
        }
        return result;
    }

    inline std::string JoinFavoriteTokens(const std::vector<std::string>& tokens)
    {
        std::string result;
        for (const auto& token : tokens) { if (!result.empty()) result += ','; result += token; }
        return result;
    }

}
