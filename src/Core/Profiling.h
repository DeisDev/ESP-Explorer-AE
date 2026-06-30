#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace ESPExplorerAE
{
    enum class ProfileMetric { CatalogCapture, CatalogInitialCapture, CatalogRefreshCapture, CatalogQuery, PluginGrouping, InventoryCapture, InventoryView,
        DetailCapture, DetailsView, LanguageLoad, FontBuild, OverlayFrame, OverlayHiddenFrame, MenuOpen, MenuClose, ReportWrite, Count };
    inline constexpr std::array<std::string_view, static_cast<std::size_t>(ProfileMetric::Count)> ProfileMetricNames{
        "catalog_capture", "catalog_initial_capture", "catalog_refresh_capture", "catalog_query", "plugin_grouping", "inventory_capture", "inventory_view",
        "detail_capture", "details_view", "language_load", "font_build", "overlay_frame", "overlay_hidden_frame", "menu_open", "menu_close", "report_write" };
    enum class ProfileGauge { ActionQueue, InputQueue, CatalogRecords, InventoryStacks, InventoryGroups,
        QueryIndexBytes, ImGuiAllocations, ImGuiFrees, ImGuiActiveAllocations, Count };
    inline constexpr std::array<std::string_view, static_cast<std::size_t>(ProfileGauge::Count)> ProfileGaugeNames{
        "action_queue", "input_queue", "catalog_records", "inventory_stacks", "inventory_groups",
        "query_index_capacity_bytes", "imgui_allocations", "imgui_frees", "imgui_active_allocations" };

    class ProfileRecorder
    {
    public:
        static constexpr std::size_t Capacity = 1024;
        struct Samples
        {
            std::array<std::uint64_t, Capacity> values{};
            std::uint64_t total{};
            std::size_t size{};
            std::size_t next{};
        };
        struct Gauge { std::uint64_t last{}, peak{}, observations{}; };
        struct Snapshot
        {
            std::uint64_t run{};
            std::array<Samples, ProfileMetricNames.size()> metrics;
            std::array<Gauge, ProfileGaugeNames.size()> gauges;
        };

        bool Configure(bool enabled)
        {
            std::lock_guard lock(mutex);
            if (Enabled() == enabled) return false;
            const auto token = epoch.load() + 1;
            if (enabled) { data = {}; data.run = token; }
            epoch = token;
            return true;
        }
        bool Enabled() const { return (epoch.load() & 1) != 0; }
        std::uint64_t Ticket() const { const auto value = epoch.load(); return (value & 1) ? value : 0; }
        void Record(ProfileMetric metric, std::uint64_t elapsed, std::uint64_t ticket)
        {
            if (!ticket) return;
            std::lock_guard lock(mutex);
            if (ticket != epoch.load()) return;
            const auto index = static_cast<std::size_t>(metric);
            if (index >= data.metrics.size()) return;
            auto& samples = data.metrics[index];
            samples.values[samples.next] = elapsed;
            samples.next = (samples.next + 1) % Capacity;
            samples.size = (std::min)(samples.size + 1, Capacity);
            ++samples.total;
        }
        void Observe(ProfileGauge gauge, std::uint64_t value)
        {
            if (!Enabled()) return;
            std::lock_guard lock(mutex);
            const auto index = static_cast<std::size_t>(gauge);
            if (!Enabled() || index >= data.gauges.size()) return;
            auto& sample = data.gauges[index];
            sample.last = value;
            sample.peak = (std::max)(sample.peak, value);
            ++sample.observations;
        }
        std::shared_ptr<const Snapshot> Read() const { std::lock_guard lock(mutex); return std::make_shared<const Snapshot>(data); }

        // Nearest-rank percentile over the retained window, not total history.
        static std::uint64_t Percentile(std::span<const std::uint64_t> sorted, std::size_t percent)
        {
            if (sorted.empty()) return 0;
            const auto rank = (sorted.size() * (std::min)(percent, std::size_t{100}) + 99) / 100;
            return sorted[rank ? rank - 1 : 0];
        }

    private:
        mutable std::mutex mutex;
        std::atomic<std::uint64_t> epoch{};
        Snapshot data{};
    };

    inline ProfileRecorder& PerformanceProfile()
    {
        // Process-lifetime instrumentation has no engine ownership or teardown.
        static auto* recorder = new ProfileRecorder;
        return *recorder;
    }

    class ProfileScope
    {
    public:
        explicit ProfileScope(ProfileMetric metric, ProfileRecorder& recorder = PerformanceProfile()) :
            recorder(recorder), metric(metric), ticket(recorder.Ticket())
        {
            if (ticket) started = std::chrono::steady_clock::now();
        }
        ProfileScope(const ProfileScope&) = delete;
        ProfileScope& operator=(const ProfileScope&) = delete;
        ~ProfileScope() noexcept
        {
            if (!ticket) return;
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count();
            try { recorder.Record(metric, static_cast<std::uint64_t>((std::max)(elapsed, std::int64_t{})), ticket); }
            catch (...) {} // Instrumentation must not throw through host callbacks.
        }
    private:
        ProfileRecorder& recorder;
        ProfileMetric metric;
        std::uint64_t ticket{};
        std::chrono::steady_clock::time_point started;
    };
}
