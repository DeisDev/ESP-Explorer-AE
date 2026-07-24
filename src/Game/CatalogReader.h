#pragma once

#include "Core/CatalogSnapshot.h"

namespace RE { class TESForm; }

namespace ESPExplorerAE
{
    // Capture must run through the game dispatcher. Returned values own their
    // strings and metadata; no RE pointers leave the adapter.
    class CatalogReader
    {
    public:
        static std::shared_ptr<CatalogSnapshot> Capture();

    private:
        static std::string GetFormName(RE::TESForm* form);
        static std::string GetSourcePluginName(RE::TESForm* form);
        static bool IsPlayable(RE::TESForm* form);
    };
}
