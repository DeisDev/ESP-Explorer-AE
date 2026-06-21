#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ESPExplorerAE
{
    struct LanguageDefinition
    {
        std::string code;
        std::string displayName;
        std::vector<std::string> fontFiles;
        std::vector<std::string> glyphRanges;
    };

    struct LanguageTable
    {
        LanguageDefinition definition{ .code = "en", .displayName = "English" };
        std::unordered_map<std::string, std::string> strings;
    };

    // All returned views borrow from this snapshot. Retain its shared owner for
    // the entire use of text or glyph samples, including across reloads.
    struct LanguageSnapshot
    {
        std::shared_ptr<const LanguageTable> selected{ std::make_shared<const LanguageTable>() };
        std::shared_ptr<const LanguageTable> english{ selected };

        std::string_view Get(std::string_view section, std::string_view key) const
        {
            const auto mapKey = std::string(section) + "." + std::string(key);
            if (const auto it = selected->strings.find(mapKey); it != selected->strings.end()) return it->second;
            if (const auto it = english->strings.find(mapKey); it != english->strings.end()) return it->second;
            return {};
        }

        const std::vector<std::string>& FontFiles() const
        {
            return selected->definition.fontFiles.empty() ? english->definition.fontFiles : selected->definition.fontFiles;
        }

        const std::vector<std::string>& GlyphRanges() const
        {
            return selected->definition.glyphRanges.empty() ? english->definition.glyphRanges : selected->definition.glyphRanges;
        }

        std::vector<std::string_view> GlyphSamples() const
        {
            std::vector<std::string_view> result;
            const auto append = [&](const LanguageTable& table) {
                for (const auto& [key, value] : table.strings) if (!value.empty()) result.push_back(value);
                if (!table.definition.displayName.empty()) result.push_back(table.definition.displayName);
            };
            append(*selected);
            if (english != selected) append(*english);
            return result;
        }
    };
}
