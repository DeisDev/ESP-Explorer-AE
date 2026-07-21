#pragma once

#include "Core/FavoriteDocument.h"
#include <SimpleIni.h>

namespace ESPExplorerAE
{
    inline bool ReadFavoriteIni(const CSimpleIniA& ini, FavoriteDocument& result, bool& legacy)
    {
        const char* schema = ini.GetValue("Favorites", "iSchema", nullptr);
        legacy = schema == nullptr;
        if (schema && std::string_view(schema) != "1") return false;
        FavoriteDocument next;
        if (legacy) next.legacy = SplitFavoriteTokens(ini.GetValue("Favorites", "sFormIDs", ""));
        else {
            next.keys = SplitFavoriteTokens(ini.GetValue("Favorites", "sKeys", ""));
            next.legacy = SplitFavoriteTokens(ini.GetValue("Favorites", "sLegacyFormIDs", ""));
        }
        result = std::move(next);
        return true;
    }

    inline void WriteFavoriteIni(CSimpleIniA& ini, const FavoriteDocument& document)
    {
        ini.SetValue("Favorites", "iSchema", "1");
        ini.SetValue("Favorites", "sKeys", JoinFavoriteTokens(document.keys).c_str());
        ini.SetValue("Favorites", "sLegacyFormIDs", JoinFavoriteTokens(document.legacy).c_str());
        // Old builds must not reinterpret pending legacy IDs automatically.
        // An exact original configuration is backed up before the first write.
        ini.Delete("Favorites", "sFormIDs");
    }
}
