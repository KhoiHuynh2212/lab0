#include "workload_generator.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace std;
using namespace workload_generator;

namespace
{
    void print_help()
    {
        cout << R"HELP(Obj-A Workload Generator

Generates a sorted synthetic workload in the CSV format used by Obj-B.

Usage:
  ./workload-generator --out <file> --interval <start,end>
                       --distribution <uniform|normal|geometric|poisson>
                       <distribution statistics> [options]

Distribution statistics:
  uniform:   --min <integer> --max <integer>
  normal:    --mean <number> --stddev <positive number>
  geometric: --probability <number in (0,1]>
  poisson:   --lambda <positive number>

Task options:
  --tasks-per-job <count>          Number of tasks in each generated job. Default: 1
  --task-work <fixed|distribution> Fixed work or work drawn from the same distribution.
                                    Default: fixed
  --fixed-task-work <positive N>   W N value used by fixed task work. Default: 10

Other options:
  --seed <integer>                 Reproduce the same random workload.
  --job-prefix <text>              Prefix for job IDs. Default: j
  --help                           Show this help text.

Examples:
  ./workload-generator --out workload.csv --interval 0,20 \
      --distribution uniform --min 0 --max 2 \
      --tasks-per-job 2 --task-work fixed --fixed-task-work 10 --seed 7

  ./workload-generator --out workload.csv --interval 0,50 \
      --distribution poisson --lambda 1.5 \
      --tasks-per-job 1 --task-work distribution --seed 42
)HELP";
    }

    map<string, string> parse_options(int argument_count, char *argument_values[])
    {
        map<string, string> options;

        for (int argument_index = 1; argument_index < argument_count; ++argument_index)
        {
            string argument_text = argument_values[argument_index];

            if (argument_text == "--help" || argument_text == "-h")
            {
                options["help"] = "true";
                continue;
            }

            if (argument_text.rfind("--", 0) != 0)
            {
                throw invalid_argument("unexpected argument: " + argument_text);
            }

            argument_text = argument_text.substr(2);
            const size_t equals_position = argument_text.find('=');

            if (equals_position != string::npos)
            {
                const string option_name = argument_text.substr(0, equals_position);
                const string option_value = argument_text.substr(equals_position + 1);
                if (option_name.empty() || option_value.empty())
                {
                    throw invalid_argument("invalid option: --" + argument_text);
                }
                options[option_name] = option_value;
                continue;
            }

            if (argument_index + 1 >= argument_count)
            {
                throw invalid_argument("missing value for --" + argument_text);
            }

            const string option_value = argument_values[++argument_index];
            if (option_value.rfind("--", 0) == 0)
            {
                throw invalid_argument("missing value for --" + argument_text);
            }

            options[argument_text] = option_value;
        }

        return options;
    }

    bool has_option(const map<string, string> &options, const string &name)
    {
        return options.find(name) != options.end();
    }

    string required_option(const map<string, string> &options, const string &name)
    {
        const auto found_option = options.find(name);
        if (found_option == options.end())
        {
            throw invalid_argument("missing required option --" + name);
        }
        return found_option->second;
    }

    string optional_option(const map<string, string> &options,
                           const string &name,
                           const string &default_value)
    {
        const auto found_option = options.find(name);
        return found_option == options.end() ? default_value : found_option->second;
    }

    long long parse_integer(const string &text, const string &description)
    {
        size_t characters_used = 0;
        long long parsed_value = 0;

        try
        {
            parsed_value = stoll(text, &characters_used);
        }
        catch (const exception &)
        {
            throw invalid_argument(description + " must be an integer: " + text);
        }

        if (characters_used != text.size())
        {
            throw invalid_argument(description + " must be an integer: " + text);
        }

        return parsed_value;
    }

    uint64_t parse_unsigned_integer(const string &text, const string &description)
    {
        if (!text.empty() && text.front() == '-')
        {
            throw invalid_argument(description + " cannot be negative: " + text);
        }

        size_t characters_used = 0;
        unsigned long long parsed_value = 0;

        try
        {
            parsed_value = stoull(text, &characters_used);
        }
        catch (const exception &)
        {
            throw invalid_argument(description + " must be a non-negative integer: " + text);
        }

        if (characters_used != text.size())
        {
            throw invalid_argument(description + " must be a non-negative integer: " + text);
        }

        return static_cast<uint64_t>(parsed_value);
    }

    double parse_number(const string &text, const string &description)
    {
        size_t characters_used = 0;
        double parsed_value = 0.0;

        try
        {
            parsed_value = stod(text, &characters_used);
        }
        catch (const exception &)
        {
            throw invalid_argument(description + " must be a number: " + text);
        }

        if (characters_used != text.size())
        {
            throw invalid_argument(description + " must be a number: " + text);
        }

        return parsed_value;
    }

    pair<long long, long long> parse_interval(const string &interval_text)
    {
        const size_t comma_position = interval_text.find(',');
        if (comma_position == string::npos || interval_text.find(',', comma_position + 1) != string::npos)
        {
            throw invalid_argument("--interval must be written as start,end");
        }

        const string start_text = interval_text.substr(0, comma_position);
        const string end_text = interval_text.substr(comma_position + 1);

        return {parse_integer(start_text, "interval start"),
                parse_integer(end_text, "interval end")};
    }

    GeneratorSettings build_settings(const map<string, string> &options)
    {
        GeneratorSettings settings;

        if (has_option(options, "interval"))
        {
            const auto [interval_start, interval_end] =
                parse_interval(required_option(options, "interval"));
            settings.interval_start = interval_start;
            settings.interval_end = interval_end;
        }
        else
        {
            settings.interval_start = parse_integer(required_option(options, "start"),
                                                    "interval start");
            settings.interval_end = parse_integer(required_option(options, "end"),
                                                  "interval end");
        }

        settings.distribution.type =
            parse_distribution_type(required_option(options, "distribution"));

        switch (settings.distribution.type)
        {
        case DistributionType::Uniform:
            settings.distribution.uniform_minimum =
                parse_integer(required_option(options, "min"), "uniform minimum");
            settings.distribution.uniform_maximum =
                parse_integer(required_option(options, "max"), "uniform maximum");
            break;

        case DistributionType::Normal:
            settings.distribution.normal_mean =
                parse_number(required_option(options, "mean"), "normal mean");
            settings.distribution.normal_standard_deviation =
                parse_number(required_option(options, "stddev"), "normal standard deviation");
            break;

        case DistributionType::Geometric:
            settings.distribution.geometric_probability =
                parse_number(required_option(options, "probability"), "geometric probability");
            break;

        case DistributionType::Poisson:
            settings.distribution.poisson_lambda =
                parse_number(required_option(options, "lambda"), "Poisson lambda");
            break;
        }

        const long long parsed_tasks_per_job =
            parse_integer(optional_option(options, "tasks-per-job", "1"),
                          "tasks per job");
        if (parsed_tasks_per_job < 0)
        {
            throw invalid_argument("tasks per job cannot be negative");
        }
        settings.tasks_per_job = static_cast<size_t>(parsed_tasks_per_job);

        const string task_work_mode = optional_option(options, "task-work", "fixed");
        if (task_work_mode == "fixed")
        {
            settings.draw_task_work_from_distribution = false;
            settings.fixed_task_work =
                parse_integer(optional_option(options, "fixed-task-work", "10"),
                              "fixed task work");
        }
        else if (task_work_mode == "distribution")
        {
            settings.draw_task_work_from_distribution = true;
        }
        else
        {
            throw invalid_argument("--task-work must be fixed or distribution");
        }

        settings.job_id_prefix = optional_option(options, "job-prefix", "j");

        if (has_option(options, "seed"))
        {
            settings.random_seed = parse_unsigned_integer(required_option(options, "seed"),
                                                          "seed");
            settings.seed_was_supplied = true;
        }

        validate_generator_settings(settings);
        return settings;
    }
}

int main(int argument_count, char *argument_values[])
{
    try
    {
        const map<string, string> options = parse_options(argument_count, argument_values);

        if (has_option(options, "help"))
        {
            print_help();
            return EXIT_SUCCESS;
        }

        const filesystem::path output_path = required_option(options, "out");
        const GeneratorSettings settings = build_settings(options);
        const vector<Job> generated_jobs = generate_workload(settings);

        write_workload_file(output_path, generated_jobs);

        cout << "Generated " << generated_jobs.size() << " jobs from time "
             << settings.interval_start << " through " << settings.interval_end
             << " using the " << distribution_type_name(settings.distribution.type)
             << " distribution.\n";
        cout << "Wrote workload to " << output_path.string() << '\n';
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << "workload-generator: " << error.what() << '\n';
        cerr << "Run workload-generator --help for usage.\n";
        return EXIT_FAILURE;
    }
}
