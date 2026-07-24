#pragma once
#include <memory>

namespace ESPExplorerAE
{
    struct CatalogSnapshot;
    struct InventorySnapshot;
    class Profiler
    {
    public:
        // Configure/Pump/Stop/SampleImGui belong to the startup/render owner.
        static void Configure(bool enabled) noexcept;
        static void Pump(bool force = false) noexcept;
        static void Stop() noexcept;
        static void SampleImGui() noexcept;
        // Game-task captures supply detached owners; the registry keeps weak references.
        static void Track(std::shared_ptr<const CatalogSnapshot> value) noexcept;
        static void Track(std::shared_ptr<const InventorySnapshot> value) noexcept;
    };
}
