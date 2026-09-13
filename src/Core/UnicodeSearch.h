#pragma once

#include "Core/UnicodeCaseData.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace ESPExplorerAE
{
    inline void AppendUTF8(std::string& result, char32_t cp)
    {
        if (cp < 0x80) result += static_cast<char>(cp);
        else if (cp < 0x800) { result += static_cast<char>(0xC0 | (cp >> 6)); result += static_cast<char>(0x80 | (cp & 63)); }
        else if (cp < 0x10000) {
            result += static_cast<char>(0xE0 | (cp >> 12)); result += static_cast<char>(0x80 | ((cp >> 6) & 63)); result += static_cast<char>(0x80 | (cp & 63));
        } else {
            result += static_cast<char>(0xF0 | (cp >> 18)); result += static_cast<char>(0x80 | ((cp >> 12) & 63));
            result += static_cast<char>(0x80 | ((cp >> 6) & 63)); result += static_cast<char>(0x80 | (cp & 63));
        }
    }
    inline std::string FoldSearchText(std::string_view text)
    {
        std::string result;
        result.reserve(text.size());
        for (std::size_t index = 0; index < text.size();) {
            const auto start = index;
            const auto first = static_cast<unsigned char>(text[index++]);
            if (first < 0x80) { result += static_cast<char>(first >= 'A' && first <= 'Z' ? first + ('a' - 'A') : first); continue; }
            char32_t cp = first;
            unsigned continuation{};
            if (first >= 0xC2 && first <= 0xDF) { cp &= 31; continuation = 1; }
            else if (first >= 0xE0 && first <= 0xEF) { cp &= 15; continuation = 2; }
            else if (first >= 0xF0 && first <= 0xF4) { cp &= 7; continuation = 3; }
            else if (first >= 0x80) { result += static_cast<char>(first); continue; }
            bool valid = index + continuation <= text.size();
            for (unsigned n = 0; valid && n < continuation; ++n) {
                const auto byte = static_cast<unsigned char>(text[index++]);
                if ((byte & 0xC0) != 0x80) valid = false;
                else cp = (cp << 6) | (byte & 63);
            }
            if (!valid || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ||
                (continuation == 1 && cp < 0x80) || (continuation == 2 && cp < 0x800) || (continuation == 3 && cp < 0x10000)) {
                index = start + 1; result += static_cast<char>(first); continue;
            }
            const auto expansion = std::ranges::lower_bound(UnicodeData::expansions, cp, {}, &UnicodeData::Expansion::source);
            if (expansion != std::end(UnicodeData::expansions) && expansion->source == cp) {
                for (unsigned n = 0; n < expansion->count; ++n) AppendUTF8(result, expansion->result[n]);
            } else {
                const auto range = std::ranges::lower_bound(UnicodeData::ranges, cp, {}, &UnicodeData::Range::last);
                if (range != std::end(UnicodeData::ranges) && cp >= range->first) cp = static_cast<char32_t>(static_cast<std::int32_t>(cp) + range->delta);
                AppendUTF8(result, cp);
            }
        }
        return result;
    }
    inline bool SearchContains(std::string_view text, std::string_view query, bool sensitive = false)
    {
        if (query.empty()) return true;
        return sensitive ? text.find(query) != std::string_view::npos : FoldSearchText(text).find(FoldSearchText(query)) != std::string::npos;
    }
}
