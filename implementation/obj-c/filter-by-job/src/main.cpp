#include "filter_by_job.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;
using namespace filter_by_job;

namespace
{
    void print_help()
    {
        cout << R"HELP(Obj-C filter-by-job

Filters event-trace log lines by job identifier.

Usage:
  ./filter-by-job <input.log> <output.log> --job=<id>[,<id>...]
  ./filter-by-job <input.log> <output.log> --jobs=<id>[,<id>...]

Options:
  --help, -h    Show this help text.

Job ids may be written with or without the leading 'j'
(for example 0 and j0 both select job j0).

Examples:
  ./filter-by-job jobin.log jobout.log --jobs=0
  ./filter-by-job jobin.log jobout.log --job=0,1,2
)HELP";
    }

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

    // Normalize "0" or "j0" to the canonical "j0" form used in event logs.
    string normalize_job_id(const string &raw_job_id)
    {
        if (raw_job_id.empty())
        {
            return raw_job_id;
        }

        if (raw_job_id[0] == 'j' || raw_job_id[0] == 'J')
        {
            return "j" + raw_job_id.substr(1);
        }

        return "j" + raw_job_id;
    }

    bool parse_job_filter(const string &argument, unordered_set<string> &allowed_jobs)
    {
        const string job_prefix = "--job=";
        const string jobs_prefix = "--jobs=";

        // Check --jobs= before --job= so the longer prefix wins.
        string job_values;
        if (argument.rfind(jobs_prefix, 0) == 0)
        {
            job_values = argument.substr(jobs_prefix.size());
        }
        else if (argument.rfind(job_prefix, 0) == 0)
        {
            job_values = argument.substr(job_prefix.size());
        }
        else
        {
            return false;
        }

        if (job_values.empty())
        {
            return false;
        }

        size_t start_index = 0;
        while (start_index <= job_values.size())
        {
            const size_t comma_index = job_values.find(',', start_index);
            const string piece = (comma_index == string::npos)
                                     ? job_values.substr(start_index)
                                     : job_values.substr(start_index, comma_index - start_index);
            const string trimmed_job_id = trim_whitespace(piece);
            if (!trimmed_job_id.empty())
            {
                allowed_jobs.insert(normalize_job_id(trimmed_job_id));
            }

            if (comma_index == string::npos)
            {
                break;
            }
            start_index = comma_index + 1;
        }

        return true;
    }
} // namespace

int main(int argument_count, char *argument_values[])
{
    try
    {
        if (argument_count >= 2)
        {
            const string first_argument = argument_values[1];
            if (first_argument == "--help" || first_argument == "-h")
            {
                print_help();
                return EXIT_SUCCESS;
            }
        }

        if (argument_count < 4)
        {
            print_help();
            return EXIT_FAILURE;
        }

        const filesystem::path input_path = argument_values[1];
        const filesystem::path output_path = argument_values[2];
        unordered_set<string> allowed_jobs;

        for (int argument_index = 3; argument_index < argument_count; ++argument_index)
        {
            const string current_argument = argument_values[argument_index];
            if (!parse_job_filter(current_argument, allowed_jobs))
            {
                throw invalid_argument("unexpected argument: " + current_argument);
            }
        }

        filter_event_log(input_path, output_path, allowed_jobs);
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
