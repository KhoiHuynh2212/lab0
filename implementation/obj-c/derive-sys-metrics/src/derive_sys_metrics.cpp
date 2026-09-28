#include "derive_sys_metrics.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace derive_sys_metrics
{
    namespace
    {
        constexpr const char *OUTPUT_HEADER =
            "time-step,Avg-response-time,median-response-time,"
            "10-response-time,90-response-time,throughput";

        string trim_whitespace(const string &input_text)
        {
            const size_t first_non_whitespace = input_text.find_first_not_of(" \t\r\n");
            if (first_non_whitespace == string::npos)
            {
                return "";
            }

            const size_t last_non_whitespace = input_text.find_last_not_of(" \t\r\n");
            return input_text.substr(first_non_whitespace,
                                     last_non_whitespace - first_non_whitespace + 1);
        }

        string strip_trailing_carriage_return(string line)
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            return line;
        }

        // Source/sink workload paths end in ".wl"; edge names do not.
        bool is_source_or_sink_path(const string &via_target)
        {
            static const string workload_suffix = ".wl";
            return via_target.size() >= workload_suffix.size() &&
                   via_target.compare(via_target.size() - workload_suffix.size(),
                                      workload_suffix.size(),
                                      workload_suffix) == 0;
        }

        long long parse_time_step(const string &field_text, size_t line_number)
        {
            const string trimmed = trim_whitespace(field_text);

            size_t consumed_characters = 0;
            long long parsed_value = 0;
            try
            {
                parsed_value = stoll(trimmed, &consumed_characters);
            }
            catch (const exception &)
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " has a non-integer timestamp");
            }

            if (consumed_characters != trimmed.size() || parsed_value < 0)
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " has an invalid timestamp");
            }

            return parsed_value;
        }

        // Round a non-negative average sum/count to the nearest integer (ties up).
        long long round_average_to_ones(long long sum, long long count)
        {
            return (sum + count / 2) / count;
        }

        // Nearest-rank percentile on a non-empty ascending sample vector.
        // rank = ceil(p/100 * n) with 0 < p <= 100, so rank is in [1, n].
        long long nearest_rank_percentile(const vector<long long> &sorted_samples,
                                          double percentile)
        {
            const size_t sample_count = sorted_samples.size();
            const size_t rank_index = static_cast<size_t>(
                ceil(percentile / 100.0 * static_cast<double>(sample_count)));
            return sorted_samples[rank_index - 1];
        }

        long long median_response_time(const vector<long long> &sorted_samples)
        {
            const size_t sample_count = sorted_samples.size();
            if (sample_count % 2 == 1)
            {
                return sorted_samples[sample_count / 2];
            }

            const long long lower_middle = sorted_samples[sample_count / 2 - 1];
            const long long upper_middle = sorted_samples[sample_count / 2];
            return round_average_to_ones(lower_middle + upper_middle, 2);
        }

        struct ExitBucket
        {
            vector<long long> response_times;
        };
    } // namespace

    void derive_system_metrics(const filesystem::path &input_path,
                               const filesystem::path &output_path)
    {
        ifstream input_file(input_path);
        if (!input_file)
        {
            throw runtime_error("cannot read input file: " + input_path.string());
        }

        string header_line;
        if (!getline(input_file, header_line))
        {
            throw runtime_error("input file is empty");
        }
        header_line = strip_trailing_carriage_return(header_line);

        unordered_map<string, long long> source_arrival_time_by_job;
        map<long long, ExitBucket> exits_by_time_step;

        string current_line;
        size_t line_number = 1;
        while (getline(input_file, current_line))
        {
            ++line_number;
            current_line = strip_trailing_carriage_return(current_line);

            if (trim_whitespace(current_line).empty())
            {
                continue;
            }

            istringstream line_stream(current_line);
            string time_token;
            string job_token;
            string in_token;
            string queue_token;
            string action_token;
            string via_target;

            if (!(line_stream >> time_token >> job_token >> in_token >> queue_token >>
                  action_token))
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " is missing required event-log fields");
            }

            const long long time_step = parse_time_step(time_token, line_number);

            if (action_token == "ARRIVE-VIA")
            {
                if (!(line_stream >> via_target))
                {
                    throw runtime_error("row " + to_string(line_number) +
                                        " ARRIVE-VIA is missing a via target");
                }

                if (!is_source_or_sink_path(via_target))
                {
                    continue;
                }

                // Keep the first source arrival for each job (system entry).
                if (source_arrival_time_by_job.find(job_token) ==
                    source_arrival_time_by_job.end())
                {
                    source_arrival_time_by_job.emplace(job_token, time_step);
                }
                continue;
            }

            if (action_token == "DEPART-VIA")
            {
                if (!(line_stream >> via_target))
                {
                    throw runtime_error("row " + to_string(line_number) +
                                        " DEPART-VIA is missing a via target");
                }

                if (!is_source_or_sink_path(via_target))
                {
                    continue;
                }

                const auto arrival_iterator = source_arrival_time_by_job.find(job_token);
                if (arrival_iterator == source_arrival_time_by_job.end())
                {
                    throw runtime_error("row " + to_string(line_number) +
                                        " DEPART-VIA for " + job_token +
                                        " has no prior source ARRIVE-VIA");
                }

                const long long response_time = time_step - arrival_iterator->second;
                if (response_time < 0)
                {
                    throw runtime_error("row " + to_string(line_number) +
                                        " has a negative response time for " + job_token);
                }

                exits_by_time_step[time_step].response_times.push_back(response_time);
                source_arrival_time_by_job.erase(arrival_iterator);
            }
        }

        ofstream output_file(output_path, ios::trunc);
        if (!output_file)
        {
            throw runtime_error("cannot write output file: " + output_path.string());
        }

        output_file << OUTPUT_HEADER << '\n';

        for (auto &time_and_bucket : exits_by_time_step)
        {
            vector<long long> &response_times = time_and_bucket.second.response_times;
            sort(response_times.begin(), response_times.end());

            long long response_sum = 0;
            for (long long response_time : response_times)
            {
                response_sum += response_time;
            }

            const long long sample_count = static_cast<long long>(response_times.size());
            const long long average_response =
                round_average_to_ones(response_sum, sample_count);
            const long long median_response = median_response_time(response_times);
            const long long percentile_10 =
                nearest_rank_percentile(response_times, 10.0);
            const long long percentile_90 =
                nearest_rank_percentile(response_times, 90.0);

            output_file << time_and_bucket.first << ',' << average_response << ','
                        << median_response << ',' << percentile_10 << ','
                        << percentile_90 << ',' << sample_count << '\n';
        }
    }

} // namespace derive_sys_metrics
