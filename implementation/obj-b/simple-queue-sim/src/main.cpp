#include "queue_sim.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace std;

namespace
{

    // Print the command-line format and the available options.
    void print_usage(ostream &output_stream)
    {
        output_stream
            << "Usage: simple-queue-sim CONFIG [TIME_LIMIT] [--time-limit N] "
               "[--metrics FILE] [--events FILE]\n"
               "\n"
               "Simulates exactly one queue-system row from CONFIG. Source/sink paths in the\n"
               "configuration are resolved relative to CONFIG. If no time limit is given, the\n"
               "simulator runs until all source jobs have arrived and the queue/servers drain.\n";
    }

    // Parse a whole number that must be zero or greater.
    long long parse_nonnegative_integer_argument(const string &argument_text,
                                                 const string &argument_name)
    {
        size_t characters_consumed = 0;
        long long parsed_value = 0;

        try
        {
            parsed_value = stoll(argument_text, &characters_consumed);
        }
        catch (const exception &)
        {
            throw runtime_error("invalid value for " + argument_name + ": " + argument_text);
        }

        // Reject values such as "10seconds" where only the beginning is a number.
        if (characters_consumed != argument_text.size() || parsed_value < 0)
        {
            throw runtime_error("invalid value for " + argument_name + ": " + argument_text);
        }

        return parsed_value;
    }

} // namespace

int main(int argument_count, char **argument_values)
{
    try
    {
        // The configuration file is the only required argument.
        if (argument_count <= 1)
        {
            print_usage(cerr);
            return 2;
        }

        const string first_argument = argument_values[1];
        if (first_argument == "--help" || first_argument == "-h")
        {
            print_usage(cout);
            return 0;
        }

        const string configuration_path_text = first_argument;
        qsim::SimulationOptions simulation_options;

        // Only one positional time limit is allowed.
        bool positional_time_limit_was_seen = false;

        for (int argument_index = 2;
             argument_index < argument_count;
             ++argument_index)
        {

            const string current_argument = argument_values[argument_index];

            if (current_argument == "--time-limit")
            {
                if (++argument_index >= argument_count)
                {
                    throw runtime_error("--time-limit requires a value");
                }

                simulation_options.time_limit =
                    parse_nonnegative_integer_argument(argument_values[argument_index],
                                                       "--time-limit");
            }
            else if (current_argument == "--metrics")
            {
                if (++argument_index >= argument_count)
                {
                    throw runtime_error("--metrics requires a filename");
                }

                simulation_options.metrics_output_path = argument_values[argument_index];
            }
            else if (current_argument == "--events")
            {
                if (++argument_index >= argument_count)
                {
                    throw runtime_error("--events requires a filename");
                }

                simulation_options.events_output_path = argument_values[argument_index];
            }
            else if (!current_argument.empty() && current_argument.front() == '-')
            {
                throw runtime_error("unknown option: " + current_argument);
            }
            else if (!positional_time_limit_was_seen &&
                     !simulation_options.time_limit)
            {
                simulation_options.time_limit =
                    parse_nonnegative_integer_argument(current_argument, "TIME_LIMIT");
                positional_time_limit_was_seen = true;
            }
            else
            {
                throw runtime_error("unexpected argument: " + current_argument);
            }
        }

        const qsim::SimulationResult simulation_result =
            qsim::run_simulation(configuration_path_text, simulation_options);

        cout << "simulation complete: "
             << simulation_result.time_steps_processed << " time steps, "
             << simulation_result.jobs_written_to_sink << " sink jobs, "
             << simulation_result.total_errors << " errors\n";

        return 0;
    }
    catch (const exception &error)
    {
        // Print the reason the program could not continue.
        cerr << "simple-queue-sim: " << error.what() << '\n';
        return 1;
    }
}
