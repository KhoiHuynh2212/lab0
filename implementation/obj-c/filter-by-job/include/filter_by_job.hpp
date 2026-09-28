#pragma once

// Filter event-trace log lines by job identifier (j<id>).

#include <filesystem>
#include <string>
#include <unordered_set>

using namespace std;

namespace filter_by_job
{

    // Keep only event-log lines whose job token is in allowed_jobs.
    // The header line ("Event Log") is always written to the output file.
    // Job ids in allowed_jobs must be normalized to the "j<id>" form.
    void filter_event_log(const filesystem::path &input_path,
                          const filesystem::path &output_path,
                          const unordered_set<string> &allowed_jobs);

}
