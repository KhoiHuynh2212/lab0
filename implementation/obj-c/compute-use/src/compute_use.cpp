#include "compute_use.hpp"

#include <cctype>
#include <deque>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace compute_use
{
    namespace
    {
        constexpr size_t WINDOW_10 = 10;
        constexpr size_t WINDOW_100 = 100;
        constexpr size_t WINDOW_1000 = 1000;

        constexpr const char *OUTPUT_HEADER =
            "time-step,Utilization(10),Saturation(10),Errors(10),"
            "Utilization(100),Saturation(100),Errors(100),"
            "Utilization(1000),Saturation(1000),Errors(1000)";

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

        // Match Obj-B / filter-by-queue header matching: ignore case and punctuation.
        string make_lowercase_alphanumeric(const string &input_text)
        {
            string normalized_text;

            for (unsigned char current_character : input_text)
            {
                if (isalnum(current_character))
                {
                    normalized_text.push_back(static_cast<char>(tolower(current_character)));
                }
            }

            return normalized_text;
        }

        vector<string> split_csv_line(const string &csv_line)
        {
            vector<string> fields;
            string current_field;
            istringstream line_stream(csv_line);

            while (getline(line_stream, current_field, ','))
            {
                fields.push_back(current_field);
            }

            // getline drops a trailing empty field after a final comma; restore it.
            if (!csv_line.empty() && csv_line.back() == ',')
            {
                fields.push_back("");
            }

            return fields;
        }

        string strip_trailing_carriage_return(string line)
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            return line;
        }

        long long parse_non_negative_integer(const string &field_text,
                                             size_t line_number,
                                             const string &field_name)
        {
            const string trimmed = trim_whitespace(field_text);
            if (trimmed.empty())
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " has an empty " + field_name + " value");
            }

            size_t consumed_characters = 0;
            long long parsed_value = 0;
            try
            {
                parsed_value = stoll(trimmed, &consumed_characters);
            }
            catch (const exception &)
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " has a non-integer " + field_name + " value");
            }

            if (consumed_characters != trimmed.size() || parsed_value < 0)
            {
                throw runtime_error("row " + to_string(line_number) +
                                    " has an invalid " + field_name + " value");
            }

            return parsed_value;
        }

        // Round a non-negative average sum/count to the nearest integer (ties up).
        // Integer-only so Valgrind and native builds agree.
        long long round_average_to_ones(long long sum, long long count)
        {
            return (sum + count / 2) / count;
        }

        // One sample stored in each sliding window.
        struct Sample
        {
            int busy_flag = 0; // 1 if num-jobs-in-service > 0, else 0
            long long jobs_in_queue = 0;
            long long errors = 0;
        };

        // Fixed-capacity sliding window with O(1) running sums.
        class SlidingWindow
        {
        public:
            explicit SlidingWindow(size_t window_capacity) : capacity_(window_capacity) {}

            void push(const Sample &sample)
            {
                samples_.push_back(sample);
                sum_busy_ += sample.busy_flag;
                sum_jobs_in_queue_ += sample.jobs_in_queue;
                sum_errors_ += sample.errors;

                if (samples_.size() > capacity_)
                {
                    const Sample &oldest = samples_.front();
                    sum_busy_ -= oldest.busy_flag;
                    sum_jobs_in_queue_ -= oldest.jobs_in_queue;
                    sum_errors_ -= oldest.errors;
                    samples_.pop_front();
                }
            }

            size_t size() const { return samples_.size(); }

            // Utilization: % of time a job is in service, rounded to ones.
            long long utilization() const
            {
                return round_average_to_ones(sum_busy_,
                                             static_cast<long long>(samples_.size()));
            }

            // Saturation: average jobs queueing, rounded to ones.
            long long saturation() const
            {
                return round_average_to_ones(sum_jobs_in_queue_,
                                             static_cast<long long>(samples_.size()));
            }

            // Errors: total errors seen in the window.
            long long errors() const { return sum_errors_; }

        private:
            size_t capacity_;
            deque<Sample> samples_;
            long long sum_busy_ = 0;
            long long sum_jobs_in_queue_ = 0;
            long long sum_errors_ = 0;
        };

        size_t find_required_column(const vector<string> &header_fields,
                                    const string &normalized_name,
                                    const string &display_name)
        {
            for (size_t column_index = 0; column_index < header_fields.size(); ++column_index)
            {
                const string normalized_header =
                    make_lowercase_alphanumeric(trim_whitespace(header_fields[column_index]));
                if (normalized_header == normalized_name)
                {
                    return column_index;
                }
            }

            throw runtime_error("input CSV is missing a " + display_name + " column");
        }

        void write_window_metrics(ostream &output, const SlidingWindow &window)
        {
            output << window.utilization() << ',' << window.saturation() << ','
                   << window.errors();
        }
    } // namespace

    void compute_use_metrics(const filesystem::path &input_path,
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

        const vector<string> header_fields = split_csv_line(header_line);
        const size_t time_step_column =
            find_required_column(header_fields, "timestep", "time-step");
        const size_t jobs_in_queue_column =
            find_required_column(header_fields, "numjobsinqueue", "num-jobs-in-queue");
        const size_t jobs_in_service_column =
            find_required_column(header_fields, "numjobsinservice", "num-jobs-in-service");
        const size_t errors_column =
            find_required_column(header_fields, "numoferrors", "Num-of-errors");

        ofstream output_file(output_path, ios::trunc);
        if (!output_file)
        {
            throw runtime_error("cannot write output file: " + output_path.string());
        }

        output_file << OUTPUT_HEADER << '\n';

        SlidingWindow window_10(WINDOW_10);
        SlidingWindow window_100(WINDOW_100);
        SlidingWindow window_1000(WINDOW_1000);

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

            const vector<string> row_fields = split_csv_line(current_line);
            const size_t required_columns =
                max(max(time_step_column, jobs_in_queue_column),
                    max(jobs_in_service_column, errors_column)) +
                1;
            if (row_fields.size() < required_columns)
            {
                throw runtime_error("row " + to_string(line_number) + " has too few columns");
            }

            const long long time_step =
                parse_non_negative_integer(row_fields[time_step_column], line_number, "time-step");
            const long long jobs_in_queue = parse_non_negative_integer(
                row_fields[jobs_in_queue_column], line_number, "num-jobs-in-queue");
            const long long jobs_in_service = parse_non_negative_integer(
                row_fields[jobs_in_service_column], line_number, "num-jobs-in-service");
            const long long errors =
                parse_non_negative_integer(row_fields[errors_column], line_number, "Num-of-errors");

            Sample sample;
            sample.busy_flag = (jobs_in_service > 0) ? 1 : 0;
            sample.jobs_in_queue = jobs_in_queue;
            sample.errors = errors;

            window_10.push(sample);
            window_100.push(sample);
            window_1000.push(sample);

            output_file << time_step << ',';
            write_window_metrics(output_file, window_10);
            output_file << ',';
            write_window_metrics(output_file, window_100);
            output_file << ',';
            write_window_metrics(output_file, window_1000);
            output_file << '\n';
        }
    }

} // namespace compute_use
