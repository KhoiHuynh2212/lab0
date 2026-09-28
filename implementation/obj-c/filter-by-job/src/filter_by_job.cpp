#include "filter_by_job.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace filter_by_job
{
    namespace
    {
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
    } // namespace

    void filter_event_log(const filesystem::path &input_path,
                          const filesystem::path &output_path,
                          const unordered_set<string> &allowed_jobs)
    {
        if (allowed_jobs.empty())
        {
            throw runtime_error("provide at least one job id with --job= or --jobs=");
        }

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

        ofstream output_file(output_path, ios::trunc);
        if (!output_file)
        {
            throw runtime_error("cannot write output file: " + output_path.string());
        }

        output_file << header_line << '\n';

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
            string time_step;
            string job_token;
            if (!(line_stream >> time_step >> job_token))
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " is missing a timestamp or job identifier");
            }

            if (allowed_jobs.find(job_token) != allowed_jobs.end())
            {
                output_file << current_line << '\n';
            }
        }
    }

} // namespace filter_by_job
