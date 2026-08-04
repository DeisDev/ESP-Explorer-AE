#include "App/DetailService.h"
#include "Core/Profiling.h"
#include "Core/DetailRefresh.h"
#include "Game/DetailReader.h"
#include "pch.h"

#include <chrono>
#include <mutex>

namespace ESPExplorerAE
{
    namespace
    {
        std::mutex detailMutex;
        DetailRefresh refresh;
        DetailRefresh::Milliseconds Now()
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        }
    }

    std::shared_ptr<const RecordDetails> DetailService::Request(DetailKey key)
    {
        std::lock_guard lock(detailMutex);
        return refresh.Request(key, Now());
    }

    void DetailService::ResetSession()
    {
        std::lock_guard lock(detailMutex);
        refresh.Clear();
    }

    void DetailService::Pump(std::uint64_t session, std::shared_ptr<const CatalogSnapshot> catalog)
    {
        std::optional<DetailRefresh::Ticket> ticket;
        {
            std::lock_guard lock(detailMutex);
            ticket = refresh.Begin(session, catalog->generation, catalog->ready, Now());
        }
        if (!ticket) return;
        const ProfileScope profileScope(ProfileMetric::DetailCapture);
        std::shared_ptr<RecordDetails> details;
        try {
            details = std::make_shared<RecordDetails>();
            details->key = ticket->key;
            details->status = DetailReadStatus::Failed;
            *details = DetailReader::Capture(ticket->key);
        } catch (const std::exception& error) {
            REX::WARN("Detail capture failed for {:08X}: {}", ticket->key.formID, error.what());
        } catch (...) {
            REX::WARN("Detail capture failed for {:08X}", ticket->key.formID);
        }
        std::lock_guard lock(detailMutex);
        refresh.Finish(*ticket, std::move(details), Now());
    }
}
