#include "pch.h"
#include "App/Profiler.h"
#include "Core/ProfileReport.h"
#include "Core/ScopeExit.h"
#include "Core/StorageEstimate.h"
#include "Platform/AtomicFile.h"
#include "Platform/FileDigest.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <chrono>

namespace ESPExplorerAE
{
    namespace
    {
        ProfileRetention retention;
        std::filesystem::path output;
        std::string identity;
        std::chrono::steady_clock::time_point nextWrite;
        std::chrono::steady_clock::time_point nextConfigureAttempt;
        bool configured{};
        bool dumping{};
    }

    void Profiler::Configure(bool enabled) noexcept
    try {
        if (configured == enabled) return;
        if (!enabled) { Stop(); return; }
        const auto now = std::chrono::steady_clock::now();
        if (now < nextConfigureAttempt) return;
        nextConfigureAttempt = now + std::chrono::seconds(5);
        HMODULE module{};
        wchar_t path[32768]{};
        std::optional<std::string> digest;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&Profiler::Configure), &module) && GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)))) digest = SHA256File(path);
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const auto capture = std::to_string(stamp) + "-" + std::to_string(GetCurrentProcessId());
        output = std::filesystem::path("Data/F4SE/Plugins") / ("ESPExplorerAE.profile-" + capture + ".json");
        SYSTEM_INFO system{};
        GetNativeSystemInfo(&system);
        identity = "{\"capture_kind\":\"runtime\",\"capture_id\":\"" + capture +
            "\",\"dll_sha256\":" + (digest ? "\"" + *digest + "\"" : "null") +
            ",\"runtime_target\":\"1.11.240\",\"f4se_version\":\"" + F4SE::GetF4SEVersion().string() +
            "\",\"plugin_version\":\"" + F4SE::GetPluginVersion().string() +
            "\",\"logical_processors\":" + std::to_string(system.dwNumberOfProcessors) +
            ",\"msvc_full_version\":" + std::to_string(_MSC_FULL_VER) + ",\"profile\":";
        retention.Clear();
        PerformanceProfile().Configure(true);
        configured = true;
        nextWrite = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        REX::INFO("Performance capture enabled: {}", output.string());
    } catch (const std::exception& error) { REX::WARN("Performance capture setup failed: {}", error.what()); }
      catch (...) { REX::WARN("Performance capture setup failed"); }

    void Profiler::Track(std::shared_ptr<const CatalogSnapshot> value) noexcept
    try {
        if (!value || !PerformanceProfile().Enabled()) return;
        PerformanceProfile().Observe(ProfileGauge::CatalogRecords, value->records.size());
        retention.Track(ProfileSnapshotKind::Catalog, value, StorageEstimate::Catalog(*value));
    } catch (...) {} // Optional instrumentation cannot cancel publication.
    void Profiler::Track(std::shared_ptr<const InventorySnapshot> value) noexcept
    try {
        if (!value || !PerformanceProfile().Enabled()) return;
        PerformanceProfile().Observe(ProfileGauge::InventoryStacks, value->stacks.size());
        PerformanceProfile().Observe(ProfileGauge::InventoryGroups, value->groups.size());
        retention.Track(ProfileSnapshotKind::Inventory, value, StorageEstimate::Inventory(*value));
    } catch (...) {}
    void Profiler::SampleImGui() noexcept
    try {
        if (!PerformanceProfile().Enabled() || !ImGui::GetCurrentContext()) return;
        const auto& allocations = ImGui::GetCurrentContext()->DebugAllocInfo;
        PerformanceProfile().Observe(ProfileGauge::ImGuiAllocations, static_cast<std::uint64_t>((std::max)(0, allocations.TotalAllocCount)));
        PerformanceProfile().Observe(ProfileGauge::ImGuiFrees, static_cast<std::uint64_t>((std::max)(0, allocations.TotalFreeCount)));
        PerformanceProfile().Observe(ProfileGauge::ImGuiActiveAllocations, static_cast<std::uint64_t>((std::max)(0, allocations.TotalAllocCount - allocations.TotalFreeCount)));
    } catch (...) {}
    void Profiler::Pump(bool force) noexcept
    try {
        if (!configured || dumping || (!force && std::chrono::steady_clock::now() < nextWrite)) return;
        dumping = true;
        ScopeExit reset([] { dumping = false; });
        nextWrite = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        const ProfileScope scope(ProfileMetric::ReportWrite);
        const auto bytes = identity + ProfileReportJSON(*PerformanceProfile().Read(), retention.Read()) + "}";
        std::string error;
        if (!WriteFileAtomically(output, bytes, error)) REX::WARN("Performance report write failed: {}", error);
    } catch (const std::exception& error) { REX::WARN("Performance report failed: {}", error.what()); }
      catch (...) { REX::WARN("Performance report failed"); }

    void Profiler::Stop() noexcept
    {
        if (!configured) return;
        Pump(true);
        try { PerformanceProfile().Configure(false); } catch (...) {}
        configured = false;
    }
}
