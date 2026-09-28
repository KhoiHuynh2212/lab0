#pragma once

// Derive system-level response-time and throughput metrics from an event log.

#include <filesystem>

using namespace std;

namespace derive_sys_metrics
{

    // Read an event-trace log and write per-exit-time-step system metrics:
    // average / median / 10th / 90th percentile response times and throughput.
    // Response time is measured from source ARRIVE-VIA (*.wl) to sink DEPART-VIA (*.wl).
    void derive_system_metrics(const filesystem::path &input_path,
                               const filesystem::path &output_path);

}
