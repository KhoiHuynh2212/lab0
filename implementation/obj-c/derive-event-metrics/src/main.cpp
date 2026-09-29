#include "derive_event_metrics.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace derive_event_metrics;

namespace
{
    void print_help()
    {
        cout << R"HELP(Obj-C derive-event-metrics

Derives per-job queue and service timing from an event log.

Usage:
  ./derive-event-metrics <jobin.log> <emetric.csv>

Options:
  --help, -h    Show this help text.

Output columns:
  job-id, queue-sys-id, arrival-time, time-in-queue, time-in-service,
  response-time    (arrival until first ENTERS-SERVER)
  turn-around-time (arrival until DEPART-VIA or ERROR)

A job that visits a queue more than once produces one row per visit.
Fields a visit never reached (for example a QUEUE-FULL job that never
entered a server) are left empty.

Example:
  ./derive-event-metrics job.log emetric.csv
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

        write_metrics(output_path, derive_visits(input_path));
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
