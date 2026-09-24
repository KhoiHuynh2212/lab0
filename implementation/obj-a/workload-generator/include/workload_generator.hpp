#pragma once

#include <cstdint>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace std;

namespace workload_generator
{

    // The generator can use any of the four distributions listed for Obj-A.
    enum class DistributionType
    {
        Uniform,
        Normal,
        Geometric,
        Poisson
    };

    // Only the fields needed by the selected distribution are used.
    struct DistributionSettings
    {
        DistributionType type = DistributionType::Uniform;

        // Uniform distribution: inclusive integer range [uniform_minimum, uniform_maximum].
        long long uniform_minimum = 0;
        long long uniform_maximum = 1;

        // Normal distribution: mean and standard deviation before rounding to an integer.
        double normal_mean = 1.0;
        double normal_standard_deviation = 1.0;

        // Geometric distribution: probability of success on each trial.
        double geometric_probability = 0.5;

        // Poisson distribution: expected number of events.
        double poisson_lambda = 1.0;
    };

    // A single task is written in the workload file as "<task-id>: W <work>".
    struct Task
    {
        string task_id;
        long long work_amount = 0;

        bool operator==(const Task &) const = default;
    };

    // A generated job contains an ID, an arrival time, and one or more tasks.
    struct Job
    {
        string job_id;
        long long arrival_time = 0;
        vector<Task> tasks;

        bool operator==(const Job &) const = default;
    };

    // Settings that describe one complete workload generation run.
    struct GeneratorSettings
    {
        long long interval_start = 0;
        long long interval_end = 0;
        DistributionSettings distribution;

        // Every generated job receives this many tasks.
        size_t tasks_per_job = 1;

        // Fixed mode uses fixed_task_work for every task.
        // Distribution mode repeatedly draws a positive value from the selected distribution.
        bool draw_task_work_from_distribution = false;
        long long fixed_task_work = 10;

        // A supplied seed makes a workload reproducible.
        uint64_t random_seed = 0;
        bool seed_was_supplied = false;

        string job_id_prefix = "j";
    };

    // Convert a distribution name such as "poisson" into the enum used internally.
    DistributionType parse_distribution_type(const string &distribution_name);
    string distribution_type_name(DistributionType distribution_type);

    // Validate all settings before random values are generated.
    void validate_generator_settings(const GeneratorSettings &settings);

    // Draw one non-negative integer from the selected distribution.
    long long draw_nonnegative_value(const DistributionSettings &settings,
                                     mt19937_64 &random_engine);

    // Draw a strictly positive integer for the W N value of a task.
    long long draw_positive_task_work(const DistributionSettings &settings,
                                      mt19937_64 &random_engine);

    // Generate the jobs for every time step in the inclusive interval.
    vector<Job> generate_workload(const GeneratorSettings &settings);

    // Convert a task list to the exact text format used by Obj-B.
    string serialize_tasks(const vector<Task> &tasks);

    // Write a workload using the three columns required by the lab.
    void write_workload_file(const filesystem::path &output_path,
                             const vector<Job> &jobs);

} // namespace workload_generator
