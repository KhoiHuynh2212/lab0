#include "workload_generator.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace workload_generator
{
    namespace
    {
        string lowercase_copy(string text)
        {
            transform(text.begin(), text.end(), text.begin(),
                      [](unsigned char character)
                      {
                          return static_cast<char>(tolower(character));
                      });
            return text;
        }

        string trim_whitespace(const string &text)
        {
            size_t first_non_space = 0;
            while (first_non_space < text.size() &&
                   isspace(static_cast<unsigned char>(text[first_non_space])))
            {
                ++first_non_space;
            }

            size_t one_past_last_non_space = text.size();
            while (one_past_last_non_space > first_non_space &&
                   isspace(static_cast<unsigned char>(text[one_past_last_non_space - 1])))
            {
                --one_past_last_non_space;
            }

            return text.substr(first_non_space,
                               one_past_last_non_space - first_non_space);
        }

        string escape_csv_field(const string &field_text)
        {
            // CSV fields only need quotes when they contain a comma, quote, or line break.
            const bool needs_quotes =
                field_text.find_first_of(",\"\r\n") != string::npos;

            if (!needs_quotes)
            {
                return field_text;
            }

            string escaped_field = "\"";
            for (char character : field_text)
            {
                if (character == '\"')
                {
                    escaped_field += "\"\"";
                }
                else
                {
                    escaped_field += character;
                }
            }
            escaped_field += '\"';
            return escaped_field;
        }

        uint64_t make_random_seed()
        {
            random_device random_device_source;

            // Combining two values gives the 64-bit engine more seed material.
            const uint64_t upper_half = static_cast<uint64_t>(random_device_source()) << 32U;
            const uint64_t lower_half = static_cast<uint64_t>(random_device_source());
            return upper_half ^ lower_half;
        }
    }

    DistributionType parse_distribution_type(const string &distribution_name)
    {
        const string normalized_name = lowercase_copy(trim_whitespace(distribution_name));

        if (normalized_name == "uniform")
        {
            return DistributionType::Uniform;
        }
        if (normalized_name == "normal")
        {
            return DistributionType::Normal;
        }
        if (normalized_name == "geometric")
        {
            return DistributionType::Geometric;
        }
        if (normalized_name == "poisson")
        {
            return DistributionType::Poisson;
        }

        throw invalid_argument("unsupported distribution: " + distribution_name);
    }

    string distribution_type_name(DistributionType distribution_type)
    {
        switch (distribution_type)
        {
        case DistributionType::Uniform:
            return "uniform";
        case DistributionType::Normal:
            return "normal";
        case DistributionType::Geometric:
            return "geometric";
        case DistributionType::Poisson:
            return "poisson";
        }

        throw invalid_argument("unknown distribution type");
    }

    void validate_generator_settings(const GeneratorSettings &settings)
    {
        if (settings.interval_start < 0 || settings.interval_end < 0)
        {
            throw invalid_argument("interval times cannot be negative");
        }
        if (settings.interval_end < settings.interval_start)
        {
            throw invalid_argument("interval end must be greater than or equal to interval start");
        }
        if (settings.tasks_per_job == 0)
        {
            throw invalid_argument("tasks per job must be at least 1");
        }
        if (settings.job_id_prefix.empty())
        {
            throw invalid_argument("job ID prefix cannot be empty");
        }
        if (!settings.draw_task_work_from_distribution && settings.fixed_task_work <= 0)
        {
            throw invalid_argument("fixed task work must be positive");
        }

        const DistributionSettings &distribution = settings.distribution;

        switch (distribution.type)
        {
        case DistributionType::Uniform:
            if (distribution.uniform_minimum < 0)
            {
                throw invalid_argument("uniform minimum cannot be negative");
            }
            if (distribution.uniform_maximum < distribution.uniform_minimum)
            {
                throw invalid_argument("uniform maximum must be at least the minimum");
            }
            if (settings.draw_task_work_from_distribution &&
                distribution.uniform_maximum == 0)
            {
                throw invalid_argument("uniform task work needs a maximum greater than 0");
            }
            break;

        case DistributionType::Normal:
            if (!isfinite(distribution.normal_mean))
            {
                throw invalid_argument("normal mean must be finite");
            }
            if (!isfinite(distribution.normal_standard_deviation) ||
                distribution.normal_standard_deviation <= 0.0)
            {
                throw invalid_argument("normal standard deviation must be positive");
            }
            break;

        case DistributionType::Geometric:
            if (!isfinite(distribution.geometric_probability) ||
                distribution.geometric_probability <= 0.0 ||
                distribution.geometric_probability > 1.0)
            {
                throw invalid_argument("geometric probability must be in (0, 1]");
            }
            if (settings.draw_task_work_from_distribution &&
                distribution.geometric_probability == 1.0)
            {
                throw invalid_argument("geometric task work cannot use probability 1 because it always draws 0");
            }
            break;

        case DistributionType::Poisson:
            if (!isfinite(distribution.poisson_lambda) ||
                distribution.poisson_lambda <= 0.0)
            {
                throw invalid_argument("Poisson lambda must be positive");
            }
            break;
        }
    }

    long long draw_nonnegative_value(const DistributionSettings &settings,
                                     mt19937_64 &random_engine)
    {
        switch (settings.type)
        {
        case DistributionType::Uniform:
        {
            uniform_int_distribution<long long> distribution(settings.uniform_minimum,
                                                             settings.uniform_maximum);
            return distribution(random_engine);
        }

        case DistributionType::Normal:
        {
            normal_distribution<double> distribution(settings.normal_mean,
                                                     settings.normal_standard_deviation);
            const double sampled_value = distribution(random_engine);

            if (!isfinite(sampled_value))
            {
                throw runtime_error("normal distribution produced a non-finite value");
            }
            if (sampled_value <= 0.0)
            {
                return 0;
            }
            if (sampled_value > static_cast<double>(numeric_limits<long long>::max()))
            {
                throw overflow_error("normal distribution produced a value that is too large");
            }

            return max(0LL, llround(sampled_value));
        }

        case DistributionType::Geometric:
        {
            geometric_distribution<long long> distribution(settings.geometric_probability);
            return distribution(random_engine);
        }

        case DistributionType::Poisson:
        {
            poisson_distribution<long long> distribution(settings.poisson_lambda);
            return distribution(random_engine);
        }
        }

        throw invalid_argument("unknown distribution type");
    }

    long long draw_positive_task_work(const DistributionSettings &settings,
                                      mt19937_64 &random_engine)
    {
        // Some valid distributions can produce zero. Tasks require a positive W N value,
        // so zero or negative results are skipped and another value is drawn.
        constexpr size_t maximum_attempts = 10000;

        for (size_t attempt_number = 0; attempt_number < maximum_attempts; ++attempt_number)
        {
            const long long sampled_work = draw_nonnegative_value(settings, random_engine);
            if (sampled_work > 0)
            {
                return sampled_work;
            }
        }

        throw runtime_error("could not draw positive task work after 10000 attempts");
    }

    vector<Job> generate_workload(const GeneratorSettings &settings)
    {
        validate_generator_settings(settings);

        const uint64_t seed_to_use = settings.seed_was_supplied
                                         ? settings.random_seed
                                         : make_random_seed();
        mt19937_64 random_engine(seed_to_use);

        vector<Job> generated_jobs;
        size_t next_job_number = 0;

        // The interval includes both endpoints. Jobs are naturally generated in arrival order.
        for (long long current_time_step = settings.interval_start;; ++current_time_step)
        {
            const long long arriving_job_count =
                draw_nonnegative_value(settings.distribution, random_engine);

            for (long long job_number_at_this_time = 0;
                 job_number_at_this_time < arriving_job_count;
                 ++job_number_at_this_time)
            {
                Job generated_job;
                generated_job.job_id = settings.job_id_prefix + to_string(next_job_number++);
                generated_job.arrival_time = current_time_step;
                generated_job.tasks.reserve(settings.tasks_per_job);

                for (size_t task_number = 0;
                     task_number < settings.tasks_per_job;
                     ++task_number)
                {
                    const long long task_work = settings.draw_task_work_from_distribution
                                                    ? draw_positive_task_work(settings.distribution,
                                                                              random_engine)
                                                    : settings.fixed_task_work;

                    generated_job.tasks.push_back(
                        {to_string(task_number), task_work});
                }

                generated_jobs.push_back(generated_job);
            }

            if (current_time_step == settings.interval_end)
            {
                break;
            }
        }

        stable_sort(generated_jobs.begin(), generated_jobs.end(),
                    [](const Job &left_job, const Job &right_job)
                    {
                        return left_job.arrival_time < right_job.arrival_time;
                    });

        return generated_jobs;
    }

    string serialize_tasks(const vector<Task> &tasks)
    {
        if (tasks.empty())
        {
            return "[]";
        }

        ostringstream serialized_tasks;
        serialized_tasks << '[';

        for (size_t task_index = 0; task_index < tasks.size(); ++task_index)
        {
            if (task_index != 0)
            {
                serialized_tasks << ", ";
            }

            serialized_tasks << '\''
                             << tasks[task_index].task_id
                             << ": W "
                             << tasks[task_index].work_amount
                             << '\'';
        }

        serialized_tasks << ']';
        return serialized_tasks.str();
    }

    void write_workload_file(const filesystem::path &output_path,
                             const vector<Job> &jobs)
    {
        if (output_path.has_parent_path())
        {
            filesystem::create_directories(output_path.parent_path());
        }

        ofstream output_file(output_path, ios::trunc);
        if (!output_file)
        {
            throw runtime_error("cannot write workload file: " + output_path.string());
        }

        output_file << "Job Id,arrival time,Tasks\n";

        for (const Job &job : jobs)
        {
            output_file << escape_csv_field(job.job_id) << ','
                        << job.arrival_time << ','
                        << escape_csv_field(serialize_tasks(job.tasks)) << '\n';
        }
    }

} // namespace workload_generator
