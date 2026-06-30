#pragma once

#include "Core/Profiling.h"
#include "Core/ProfileRetention.h"
#include <sstream>
#include <locale>
#include <string>

namespace ESPExplorerAE
{
    inline std::string ProfileReportJSON(const ProfileRecorder::Snapshot& data, const ProfileRetention::Snapshot& retention)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << "{\"schema\":1,\"timing_unit\":\"ns\",\"timing_basis\":\"cpu_scope_elapsed_including_scheduling\","
            "\"gpu_timing_available\":false,\"whole_heap_allocations_available\":false,\"window_capacity\":" << ProfileRecorder::Capacity <<
            ",\"run\":" << data.run << ",\"metrics\":{";
        for (std::size_t i = 0; i < data.metrics.size(); ++i) {
            if (i) out << ',';
            const auto& metric = data.metrics[i];
            std::vector<std::uint64_t> sorted(metric.values.begin(), metric.values.begin() + metric.size);
            std::ranges::sort(sorted);
            out << '"' << ProfileMetricNames[i] << "\":{\"total\":" << metric.total << ",\"count\":" << metric.size <<
                ",\"p50\":" << ProfileRecorder::Percentile(sorted, 50) << ",\"p95\":" << ProfileRecorder::Percentile(sorted, 95) <<
                ",\"p99\":" << ProfileRecorder::Percentile(sorted, 99) << ",\"samples\":[";
            for (std::size_t j = 0; j < sorted.size(); ++j) { if (j) out << ','; out << sorted[j]; }
            out << "]}";
        }
        out << "},\"gauges\":{";
        for (std::size_t i = 0; i < data.gauges.size(); ++i) {
            if (i) out << ',';
            const auto& gauge = data.gauges[i];
            out << '"' << ProfileGaugeNames[i] << "\":{\"last\":" << gauge.last << ",\"peak\":" << gauge.peak << ",\"observations\":" << gauge.observations << '}';
        }
        out << "},\"retention\":{\"registry_capacity\":" << ProfileRetention::Capacity << ",\"dropped\":" << retention.dropped;
        constexpr const char* names[]{ "catalog", "inventory" };
        for (std::size_t i = 0; i < retention.live.size(); ++i)
            out << ",\"" << names[i] << "\":{\"live\":" << retention.live[i].objects << ",\"estimated_bytes\":" << retention.live[i].estimatedBytes << '}';
        out << "}}";
        return out.str();
    }
}
