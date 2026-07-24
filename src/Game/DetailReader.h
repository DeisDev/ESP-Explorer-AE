#pragma once
#include "Core/RecordDetails.h"

namespace ESPExplorerAE
{
    class DetailReader
    {
    public:
        // Only the serialized game-operation context may read engine objects.
        // No engine pointer, string view, localization or UI state escapes.
        static RecordDetails Capture(DetailKey key);
    };
}
