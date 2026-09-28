#pragma once

#include <filesystem>
#include <string>
#include <vector>

using namespace std;

namespace derive_event_metrics
{

    // One visit of one job to one queue = one row of emetric.csv.
    struct JobVisit
    {
        string job_id;
        string queue_id;
        long arrival_time = -1;
        long first_enter_time = -1;
        long current_enter_time = -1;
        long time_in_service = 0;
        long depart_time = -1;
    };

    // One JobVisit per ARRIVE-VIA, in the order the visits ended.
    vector<JobVisit> derive_visits(const filesystem::path &input_path);

    void write_metrics(const filesystem::path &output_path,
                       const vector<JobVisit> &visits);

}
