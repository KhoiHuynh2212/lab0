#pragma once

#include "queue_sim.hpp"

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace std;

namespace mqsim
{

    struct QueueSystemConfig
    {
        string queue_system_id;
        size_t maximum_queue_size = 0;
        size_t number_of_servers = 0;

        string server_function_text;
        qsim::ServerFunctionChain server_function_chain;
        long long server_function_rate = 0;

        string source_workload_path;
        string sink_workload_path;

        vector<string> incoming_edges;
        vector<string> outgoing_edges;
    };

    // A full multi-queue network: one row per queue-system.
    struct MultiQueueConfig
    {
        vector<QueueSystemConfig> queue_systems;
    };

    struct SimulationOptions
    {
        optional<long long> time_limit;

        filesystem::path metrics_output_path = "qmet.csv";
        filesystem::path events_output_path = "job.log";
    };

    // Per-queue-system counters, plus totals, returned when the run finishes.
    struct QueueSystemResult
    {
        string queue_system_id;
        size_t jobs_written_to_sink = 0;
        size_t jobs_departed_via_outgoing_edge = 0;
        size_t jobs_left_waiting = 0;
        size_t jobs_left_in_service = 0;
        size_t source_jobs_not_yet_arrived = 0;
        size_t errors = 0;
    };

    struct SimulationResult
    {
        long long time_steps_processed = 0;
        size_t total_errors = 0;
        vector<QueueSystemResult> per_queue_system;
    };

    MultiQueueConfig load_multi_queue_config(const filesystem::path &configuration_path);


    SimulationResult run_multi_simulation(const filesystem::path &configuration_path,
                                           const SimulationOptions &simulation_options);

} // namespace mqsim
