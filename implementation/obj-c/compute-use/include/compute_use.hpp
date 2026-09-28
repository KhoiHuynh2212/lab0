#pragma once

// Compute USE-method rolling metrics from a queue-metrics CSV.

#include <filesystem>

using namespace std;

namespace compute_use
{

    // Read chronological queue-metrics rows and write sliding-window
    // Utilization / Saturation / Errors for windows of size 10, 100, and 1000.
    void compute_use_metrics(const filesystem::path &input_path,
                             const filesystem::path &output_path);

}
