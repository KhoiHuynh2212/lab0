#pragma once

// Data structures and functions for the Obj-B simple queue simulator.

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using namespace std;

namespace qsim
{

    // One task in a job.
    // Example: "0: W 10" has task ID "0" and 10 units of work left.
    struct Task
    {
        string task_id;
        long long remaining_work = 0;

        bool operator==(const Task &) const = default;
    };

    // One job moving through the queue system.
    struct Job
    {
        string job_id;

        // For a source job, this is when the job arrives.
        // For a sink job, this is when the job leaves the system.
        long long arrival_time_step = 0;

        vector<Task> tasks;

        bool operator==(const Job &) const = default;
    };

    // A parsed server function such as Sub-10 or Sub-5,Sub-10*.
    struct ServerFunctionChain
    {
        // Each value is the amount subtracted by one Sub-X step.
        vector<long long> subtraction_amounts;

        // A trailing '*' repeats the chain until the job has no tasks left.
        bool repeat_until_tasks_complete = false;
    };

    // Settings read from the queue configuration CSV.
    struct QueueConfig
    {
        string queue_system_id;
        size_t maximum_queue_size = 0;
        size_t number_of_servers = 0;

        // Keep both the original function text and its parsed form.
        string server_function_text;
        ServerFunctionChain server_function_chain;
        long long server_function_rate = 0;

        string source_workload_path;
        string sink_workload_path;

        vector<string> incoming_edges;
        vector<string> outgoing_edges;
    };

    // Options that control one simulation run.
    struct SimulationOptions
    {
        // If this is empty, the simulation runs until all work is finished.
        optional<long long> time_limit;

        filesystem::path metrics_output_path = "qmet.csv";
        filesystem::path events_output_path = "job.log";
    };

    // Basic information returned when the simulation finishes.
    struct SimulationResult
    {
        long long time_steps_processed = 0;
        size_t total_errors = 0;
        size_t jobs_written_to_sink = 0;
        size_t jobs_departed_via_outgoing_edge = 0;
        size_t jobs_left_waiting = 0;
        size_t jobs_left_in_service = 0;
        size_t source_jobs_not_yet_arrived = 0;
    };

    // Parse one CSV line into fields.
    vector<string> parse_csv_line(const string &csv_line);

    // Convert a task-list field to tasks and back again.
    vector<Task> parse_tasks(const string &task_list_text);
    string serialize_tasks(const vector<Task> &tasks);

    // Parse server functions and edge lists from the configuration file.
    ServerFunctionChain parse_server_function(const string &server_function_text);
    vector<string> parse_edge_list(const string &edge_list_text);

    // Read and write the queue configuration and workload files.
    QueueConfig load_queue_config(const filesystem::path &configuration_path);
    vector<Job> load_workload(const filesystem::path &workload_path);
    void write_workload(const filesystem::path &workload_path, const vector<Job> &jobs);

    // Run one simple queue system.
    SimulationResult run_simulation(const filesystem::path &configuration_path,
                                    const SimulationOptions &simulation_options);

} // namespace qsim
