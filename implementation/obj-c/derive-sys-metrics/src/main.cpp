#include "derive_sys_metrics.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace derive_sys_metrics;

namespace
{
    void print_help()
    {
        cout << R"HELP(Obj-C derive-sys-metrics

Derives system response-time and throughput metrics from an event log.
Jobs are tracked from source ARRIVE-VIA (*.wl) to sink DEPART-VIA (*.wl).

Usage:
  ./derive-sys-metrics <input.log> <output.csv>

Options:
  --help, -h    Show this help text.

Examples:
  ./derive-sys-metrics jobin.log sysmetric.csv
)HELP";
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

        if (argument_count != 3)
        {
            print_help();
            return EXIT_FAILURE;
        }

        const filesystem::path input_path = argument_values[1];
        const filesystem::path output_path = argument_values[2];

        derive_system_metrics(input_path, output_path);
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
