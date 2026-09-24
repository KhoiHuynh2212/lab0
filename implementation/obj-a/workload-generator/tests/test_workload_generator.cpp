#include "workload_generator.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace workload_generator;

namespace
{
    int checks_run = 0;

    void require(bool condition, const string &message)
    {
        ++checks_run;
        if (!condition)
        {
            throw runtime_error("test failed: " + message);
        }
    }

    template <typename Callable>
    void require_throws(Callable callable, const string &message)
    {
        ++checks_run;
        try
        {
            callable();
        }
        catch (const exception &)
        {
            return;
        }

        throw runtime_error("test failed: expected exception: " + message);
    }

    string read_file(const filesystem::path &path)
    {
        ifstream input_file(path);
        if (!input_file)
        {
            throw runtime_error("could not read test file: " + path.string());
        }

        ostringstream contents;
        contents << input_file.rdbuf();
        return contents.str();
    }
}

int main()
{
    try
    {
        require(parse_distribution_type("uniform") == DistributionType::Uniform,
                "uniform distribution name");
        require(parse_distribution_type("NORMAL") == DistributionType::Normal,
                "normal distribution name is case-insensitive");
        require(parse_distribution_type(" geometric ") == DistributionType::Geometric,
                "geometric distribution name ignores surrounding spaces");
        require(parse_distribution_type("Poisson") == DistributionType::Poisson,
                "Poisson distribution name");
        require_throws([]
                       { parse_distribution_type("unknown"); },
                       "unknown distribution is rejected");

        require(distribution_type_name(DistributionType::Uniform) == "uniform",
                "uniform enum name");
        require(distribution_type_name(DistributionType::Normal) == "normal",
                "normal enum name");
        require(distribution_type_name(DistributionType::Geometric) == "geometric",
                "geometric enum name");
        require(distribution_type_name(DistributionType::Poisson) == "poisson",
                "Poisson enum name");

        GeneratorSettings deterministic_settings;
        deterministic_settings.interval_start = 2;
        deterministic_settings.interval_end = 4;
        deterministic_settings.distribution.type = DistributionType::Uniform;
        deterministic_settings.distribution.uniform_minimum = 2;
        deterministic_settings.distribution.uniform_maximum = 2;
        deterministic_settings.tasks_per_job = 2;
        deterministic_settings.fixed_task_work = 10;
        deterministic_settings.seed_was_supplied = true;
        deterministic_settings.random_seed = 1234;

        const vector<Job> deterministic_jobs = generate_workload(deterministic_settings);
        require(deterministic_jobs.size() == 6, "two jobs arrive at each of three time steps");
        require(deterministic_jobs[0].job_id == "j0", "first generated job ID");
        require(deterministic_jobs[5].job_id == "j5", "last generated job ID");
        require(deterministic_jobs[0].arrival_time == 2, "first arrival time");
        require(deterministic_jobs[2].arrival_time == 3, "middle arrival time");
        require(deterministic_jobs[5].arrival_time == 4, "last arrival time");
        require(deterministic_jobs[0].tasks.size() == 2, "tasks per job");
        require(deterministic_jobs[0].tasks[0] == Task{"0", 10}, "first task format");
        require(deterministic_jobs[0].tasks[1] == Task{"1", 10}, "second task format");
        require(serialize_tasks(deterministic_jobs[0].tasks) ==
                    "['0: W 10', '1: W 10']",
                "task list serialization");
        require(serialize_tasks({}) == "[]", "empty task list serialization");

        GeneratorSettings distributed_task_settings;
        distributed_task_settings.interval_start = 0;
        distributed_task_settings.interval_end = 1;
        distributed_task_settings.distribution.type = DistributionType::Uniform;
        distributed_task_settings.distribution.uniform_minimum = 1;
        distributed_task_settings.distribution.uniform_maximum = 1;
        distributed_task_settings.tasks_per_job = 3;
        distributed_task_settings.draw_task_work_from_distribution = true;
        distributed_task_settings.seed_was_supplied = true;
        distributed_task_settings.random_seed = 9;

        const vector<Job> distributed_task_jobs = generate_workload(distributed_task_settings);
        require(distributed_task_jobs.size() == 2, "distribution mode still draws job counts");
        require(distributed_task_jobs[0].tasks.size() == 3, "three distributed tasks are created");
        require(distributed_task_jobs[0].tasks[2].work_amount == 1,
                "task work is drawn from the selected distribution");

        GeneratorSettings poisson_settings;
        poisson_settings.interval_start = 0;
        poisson_settings.interval_end = 10;
        poisson_settings.distribution.type = DistributionType::Poisson;
        poisson_settings.distribution.poisson_lambda = 2.0;
        poisson_settings.tasks_per_job = 1;
        poisson_settings.fixed_task_work = 5;
        poisson_settings.seed_was_supplied = true;
        poisson_settings.random_seed = 77;

        const vector<Job> first_seeded_run = generate_workload(poisson_settings);
        const vector<Job> second_seeded_run = generate_workload(poisson_settings);
        require(first_seeded_run == second_seeded_run,
                "same seed reproduces the same workload");

        mt19937_64 random_engine(42);
        DistributionSettings normal_distribution_settings;
        normal_distribution_settings.type = DistributionType::Normal;
        normal_distribution_settings.normal_mean = -100.0;
        normal_distribution_settings.normal_standard_deviation = 0.1;
        require(draw_nonnegative_value(normal_distribution_settings, random_engine) == 0,
                "negative normal samples are clamped to zero jobs");

        DistributionSettings geometric_distribution_settings;
        geometric_distribution_settings.type = DistributionType::Geometric;
        geometric_distribution_settings.geometric_probability = 1.0;
        require(draw_nonnegative_value(geometric_distribution_settings, random_engine) == 0,
                "geometric p=1 produces zero failures");

        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.interval_start = 5;
                           invalid.interval_end = 4;
                           validate_generator_settings(invalid); },
                       "reversed interval");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.tasks_per_job = 0;
                           validate_generator_settings(invalid); },
                       "zero tasks per job");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.fixed_task_work = 0;
                           validate_generator_settings(invalid); },
                       "non-positive fixed task work");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.distribution.type = DistributionType::Uniform;
                           invalid.distribution.uniform_minimum = 3;
                           invalid.distribution.uniform_maximum = 2;
                           validate_generator_settings(invalid); },
                       "invalid uniform range");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.distribution.type = DistributionType::Normal;
                           invalid.distribution.normal_standard_deviation = 0.0;
                           validate_generator_settings(invalid); },
                       "normal standard deviation must be positive");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.distribution.type = DistributionType::Geometric;
                           invalid.distribution.geometric_probability = 0.0;
                           validate_generator_settings(invalid); },
                       "geometric probability lower bound");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.distribution.type = DistributionType::Poisson;
                           invalid.distribution.poisson_lambda = 0.0;
                           validate_generator_settings(invalid); },
                       "Poisson lambda must be positive");
        require_throws([]
                       {
                           GeneratorSettings invalid;
                           invalid.distribution.type = DistributionType::Geometric;
                           invalid.distribution.geometric_probability = 1.0;
                           invalid.draw_task_work_from_distribution = true;
                           validate_generator_settings(invalid); },
                       "geometric p=1 cannot provide positive task work");

        const filesystem::path temporary_directory =
            filesystem::temp_directory_path() / "obj_a_workload_generator_tests";
        filesystem::remove_all(temporary_directory);
        filesystem::create_directories(temporary_directory);

        const filesystem::path workload_path = temporary_directory / "nested" / "workload.csv";
        write_workload_file(workload_path, deterministic_jobs);
        const string workload_text = read_file(workload_path);

        require(workload_text.find("Job Id,arrival time,Tasks\n") == 0,
                "workload header");
        require(workload_text.find("j0,2,\"['0: W 10', '1: W 10']\"") != string::npos,
                "workload row uses Obj-B-compatible task text");
        require(workload_text.find("j5,4,\"['0: W 10', '1: W 10']\"") != string::npos,
                "workload contains the final job");

        filesystem::remove_all(temporary_directory);

        cout << "PASS: " << checks_run << " workload-generator checks\n";
        return 0;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return 1;
    }
}
