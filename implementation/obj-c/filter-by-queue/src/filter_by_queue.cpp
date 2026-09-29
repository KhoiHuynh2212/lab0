#include "filter_by_queue.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace std;

namespace filter_by_queue
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

        // Match Obj-B header matching: ignore case and punctuation.
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
    } // namespace

    void filter_metrics_file(const filesystem::path &input_path,
                             const filesystem::path &output_path,
                             const unordered_set<string> &allowed_queues)
    {
        if (allowed_queues.empty())
        {
            throw runtime_error("provide at least one queue id with --queue= or --queues=");
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

        const vector<string> header_fields = split_csv_line(header_line);
        size_t queue_column_index = header_fields.size();

        for (size_t column_index = 0; column_index < header_fields.size(); ++column_index)
        {
            const string normalized_header =
                make_lowercase_alphanumeric(trim_whitespace(header_fields[column_index]));
            if (normalized_header == "queuesysid")
            {
                queue_column_index = column_index;
                break;
            }
        }

        if (queue_column_index == header_fields.size())
        {
            throw runtime_error("input CSV is missing a queue-sys-id column");
        }

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

            const vector<string> row_fields = split_csv_line(current_line);
            if (queue_column_index >= row_fields.size())
            {
                throw runtime_error("row " + to_string(line_number) + " has too few columns");
            }

            const string queue_id = trim_whitespace(row_fields[queue_column_index]);
            if (allowed_queues.find(queue_id) != allowed_queues.end())
            {
                output_file << current_line << '\n';
            }
        }
    }

} // namespace filter_by_queue
