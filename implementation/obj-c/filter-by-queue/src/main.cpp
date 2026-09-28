#include "filter_by_queue.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;
using namespace filter_by_queue;

namespace
{
    void print_help()
    {
        cout << R"HELP(Obj-C filter-by-queue

Filters queue metrics CSV rows by queue-sys-id.

Usage:
  ./filter-by-queue <input.csv> <output.csv> --queue=<id>[,<id>...]
  ./filter-by-queue <input.csv> <output.csv> --queues=<id>[,<id>...]

Options:
  --help, -h    Show this help text.

Examples:
  ./filter-by-queue qmetin.csv qmetout.csv --queues=0
  ./filter-by-queue qmetin.csv qmetout.csv --queue=0,2,3
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

    bool parse_queue_filter(const string &argument, unordered_set<string> &allowed_queues)
    {
        const string queue_prefix = "--queue=";
        const string queues_prefix = "--queues=";

        // Check --queues= before --queue= so the longer prefix wins.
        string queue_values;
        if (argument.rfind(queues_prefix, 0) == 0)
        {
            queue_values = argument.substr(queues_prefix.size());
        }
        else if (argument.rfind(queue_prefix, 0) == 0)
        {
            queue_values = argument.substr(queue_prefix.size());
        }
        else
        {
            return false;
        }

        if (queue_values.empty())
        {
            return false;
        }

        size_t start_index = 0;
        while (start_index <= queue_values.size())
        {
            const size_t comma_index = queue_values.find(',', start_index);
            const string piece = (comma_index == string::npos)
                                     ? queue_values.substr(start_index)
                                     : queue_values.substr(start_index, comma_index - start_index);
            const string trimmed_queue_id = trim_whitespace(piece);
            if (!trimmed_queue_id.empty())
            {
                allowed_queues.insert(trimmed_queue_id);
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
        unordered_set<string> allowed_queues;

        for (int argument_index = 3; argument_index < argument_count; ++argument_index)
        {
            const string current_argument = argument_values[argument_index];
            if (!parse_queue_filter(current_argument, allowed_queues))
            {
                throw invalid_argument("unexpected argument: " + current_argument);
            }
        }

        filter_metrics_file(input_path, output_path, allowed_queues);
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
