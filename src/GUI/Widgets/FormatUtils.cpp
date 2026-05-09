#include "GUI/Widgets/FormatUtils.h"

#include <cstdio>

namespace ESPExplorerAE::FormatUtils
{
    namespace
    {
        void AppendQuoted(std::string& text, const std::string& value)
        {
            text.push_back('"');
            for (const char ch : value) {
                if (ch == '"') {
                    text.push_back('"');
                }
                text.push_back(ch);
            }
            text.push_back('"');
        }
    }

    std::string FormID(std::uint32_t formID)
    {
        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "%08X", formID);
        return buffer;
    }

    std::string MultiCopyList(std::span<const std::string> values, MultiCopyFormat format)
    {
        std::string text{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i > 0) {
                switch (format) {
                case MultiCopyFormat::Lines:
                    text.push_back('\n');
                    break;
                case MultiCopyFormat::CommaSeparated:
                case MultiCopyFormat::Parenthesized:
                case MultiCopyFormat::QuotedCommaSeparated:
                    text += ", ";
                    break;
                }
            }

            switch (format) {
            case MultiCopyFormat::Lines:
            case MultiCopyFormat::CommaSeparated:
                text += values[i];
                break;
            case MultiCopyFormat::Parenthesized:
                text += "(";
                text += values[i];
                text += ")";
                break;
            case MultiCopyFormat::QuotedCommaSeparated:
                AppendQuoted(text, values[i]);
                break;
            }
        }
        return text;
    }
}
