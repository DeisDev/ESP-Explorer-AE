#include "pch.h"
#include "App/CatalogService.h"
#include "App/Profiler.h"
#include "Core/Profiling.h"
#include "Core/SnapshotStore.h"
#include "Core/CatalogRefresh.h"
#include "Game/CatalogReader.h"

#include <chrono>

namespace ESPExplorerAE
{
    namespace
    {
        SnapshotStore<CatalogSnapshot> publication;
        std::mutex stateMutex;
        CatalogRefresh refresh;
        std::uint64_t generation{};
        std::chrono::steady_clock::time_point retryAfter{};
    }

    void CatalogService::RequestRefresh()
    {
        std::lock_guard lock(stateMutex);
        refresh.Request();
    }

    void CatalogService::SetAvailable(bool available, bool waitForWorld)
    {
        std::lock_guard lock(stateMutex);
        refresh.SetAvailable(available, waitForWorld);
        if (!available && publication.Read()->ready) {
            auto empty = std::make_shared<CatalogSnapshot>();
            empty->generation = ++generation;
            publication.Publish(std::move(empty));
        }
        retryAfter = {};
    }

    void CatalogService::Pump(bool worldReady)
    {
        std::optional<std::uint64_t> ticket;
        {
            std::lock_guard lock(stateMutex);
            if (std::chrono::steady_clock::now() < retryAfter) return;
            ticket = refresh.Begin(worldReady);
        }
        if (!ticket) return;
        const ProfileScope profileScope(publication.Read()->ready ? ProfileMetric::CatalogRefreshCapture : ProfileMetric::CatalogInitialCapture);
        const auto started = std::chrono::steady_clock::now();
        try {
            auto captured = CatalogReader::Capture();
            Profiler::Track(captured);
            std::lock_guard lock(stateMutex);
            if (!refresh.Finish(*ticket, captured != nullptr)) {
                if (!captured) retryAfter = started + std::chrono::seconds(2);
                return;
            }
            captured->generation = ++generation;
            publication.Publish(captured);
            REX::INFO("Catalog generation {}: {} canonical records captured in {} ms on thread {}", generation, captured->records.size(),
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(), GetCurrentThreadId());
        } catch (const std::exception& error) {
            std::lock_guard lock(stateMutex);
            refresh.Finish(*ticket, false);
            retryAfter = started + std::chrono::seconds(2);
            REX::WARN("Catalog capture failed; previous snapshot retained: {}", error.what());
        } catch (...) {
            std::lock_guard lock(stateMutex);
            refresh.Finish(*ticket, false);
            retryAfter = started + std::chrono::seconds(2);
            REX::WARN("Catalog capture failed with an unknown exception; previous snapshot retained");
        }
    }

    std::shared_ptr<const CatalogSnapshot> CatalogService::Read() { return publication.Read(); }
}
