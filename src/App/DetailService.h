#pragma once

#include "Core/CatalogSnapshot.h"
#include "Core/RecordDetails.h"

namespace ESPExplorerAE
{
    class DetailService
    {
    public:
        // Render context: request/read detached values only, with one bounded
        // latest-selection slot shared by the mutually exclusive detail views.
        static std::shared_ptr<const RecordDetails> Request(DetailKey key);
        static void ResetSession();
        // Called only by the application under its game-operation execution gate.
        static void Pump(std::uint64_t session, std::shared_ptr<const CatalogSnapshot> catalog);
    };
}
