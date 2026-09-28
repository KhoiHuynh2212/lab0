#include "compute_use.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace compute_use;

namespace
{
    void print_help()
    {
        cout << R"HELP(Obj-C compute-use

Computes USE-method Utilization, Saturation, and Errors over
sliding windows of 10, 100, and 1000 time steps.

Usage:
  ./compute-use <input.csv> <output.csv>

Options:
  --help, -h    Show this help text.

Examples:
  ./compute-use qmet.csv quse.csv
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

        compute_use_metrics(input_path, output_path);
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
