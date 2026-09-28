#pragma once

// Filter queue-metrics CSV rows by queue-sys-id.

#include <filesystem>
#include <string>
#include <unordered_set>

using namespace std;

namespace filter_by_queue
{

    // Keep only rows whose queue-sys-id is in allowed_queues.
    // The header row is always written to the output file.
    void filter_metrics_file(const filesystem::path &input_path,
                             const filesystem::path &output_path,
                             const unordered_set<string> &allowed_queues);

}
