#include "workload_visualizer.hpp"

#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace std;
using namespace workload_visualizer;

namespace
{
    struct ParsedCommandLine
    {
        filesystem::path workload_path;
        map<string, string> options;
    };

    void print_help()
    {
        cout << R"HELP(Obj-A Workload Visualizer

Creates a PNG histogram or scatter plot from a workload CSV file.

Usage:
  ./workload-visualizer <workload.csv> --hist="arrival time" --out=plot.png [options]
  ./workload-visualizer <workload.csv> --scat="arrival time,len(Tasks)" --out=plot.png [options]

Plot modes:
  --hist <column>              Histogram of one numeric column.
  --scat <x-column,y-column>   Scatter plot comparing two numeric columns.

Supported numeric workload values:
  arrival time
  len(Tasks)

Options:
  --out <file.png>             Output PNG file. Required.
  --bins <count>               Histogram bin count. Default: 10
  --xlim <min,max>             Optional x-axis limits.
  --ylim <min,max>             Optional y-axis limits.
  --title <text>               Optional plot title.
  --width <pixels>             PNG width. Default: 1000
  --height <pixels>            PNG height. Default: 700
  --help                       Show this help text.

Examples:
  ./workload-visualizer workload.csv \
      --hist="arrival time" --bins=20 --out=arrival-histogram.png

  ./workload-visualizer workload.csv \
      --scat="arrival time,len(Tasks)" \
      --title="Arrival Time vs Task Count" \
      --out=arrival-vs-tasks.png
)HELP";
    }

    ParsedCommandLine parse_command_line(int argument_count, char *argument_values[])
    {
        ParsedCommandLine parsed_command_line;
        bool workload_path_was_set = false;

        for (int argument_index = 1; argument_index < argument_count; ++argument_index)
        {
            string argument_text = argument_values[argument_index];

            if (argument_text == "--help" || argument_text == "-h")
            {
                parsed_command_line.options["help"] = "true";
                continue;
            }

            if (argument_text.rfind("--", 0) != 0)
            {
                if (workload_path_was_set)
                {
                    throw invalid_argument("unexpected positional argument: " + argument_text);
                }

                parsed_command_line.workload_path = argument_text;
                workload_path_was_set = true;
                continue;
            }

            argument_text = argument_text.substr(2);
            const size_t equals_position = argument_text.find('=');

            if (equals_position != string::npos)
            {
                const string option_name = argument_text.substr(0, equals_position);
                const string option_value = argument_text.substr(equals_position + 1);
                if (option_name.empty() || option_value.empty())
                {
                    throw invalid_argument("invalid option: --" + argument_text);
                }
                parsed_command_line.options[option_name] = option_value;
                continue;
            }

            if (argument_index + 1 >= argument_count)
            {
                throw invalid_argument("missing value for --" + argument_text);
            }

            const string option_value = argument_values[++argument_index];
            if (option_value.rfind("--", 0) == 0)
            {
                throw invalid_argument("missing value for --" + argument_text);
            }

            parsed_command_line.options[argument_text] = option_value;
        }

        return parsed_command_line;
    }

    bool has_option(const map<string, string> &options, const string &name)
    {
        return options.find(name) != options.end();
    }

    string required_option(const map<string, string> &options, const string &name)
    {
        const auto found_option = options.find(name);
        if (found_option == options.end())
        {
            throw invalid_argument("missing required option --" + name);
        }
        return found_option->second;
    }

    string optional_option(const map<string, string> &options,
                           const string &name,
                           const string &default_value)
    {
        const auto found_option = options.find(name);
        return found_option == options.end() ? default_value : found_option->second;
    }

    long long parse_integer(const string &text, const string &description)
    {
        size_t characters_used = 0;
        long long parsed_value = 0;

        try
        {
            parsed_value = stoll(text, &characters_used);
        }
        catch (const exception &)
        {
            throw invalid_argument(description + " must be an integer: " + text);
        }

        if (characters_used != text.size())
        {
            throw invalid_argument(description + " must be an integer: " + text);
        }

        return parsed_value;
    }

    double parse_number(const string &text, const string &description)
    {
        size_t characters_used = 0;
        double parsed_value = 0.0;

        try
        {
            parsed_value = stod(text, &characters_used);
        }
        catch (const exception &)
        {
            throw invalid_argument(description + " must be a number: " + text);
        }

        if (characters_used != text.size())
        {
            throw invalid_argument(description + " must be a number: " + text);
        }

        return parsed_value;
    }

    pair<double, double> parse_limits(const string &text, const string &description)
    {
        const size_t comma_position = text.find(',');
        if (comma_position == string::npos || text.find(',', comma_position + 1) != string::npos)
        {
            throw invalid_argument(description + " must be written as min,max");
        }

        return {parse_number(text.substr(0, comma_position), description + " minimum"),
                parse_number(text.substr(comma_position + 1), description + " maximum")};
    }

    pair<string, string> parse_scatter_columns(const string &text)
    {
        const size_t comma_position = text.find(',');
        if (comma_position == string::npos || text.find(',', comma_position + 1) != string::npos)
        {
            throw invalid_argument("--scat must contain two column names separated by one comma");
        }

        const string x_column = text.substr(0, comma_position);
        const string y_column = text.substr(comma_position + 1);

        if (x_column.empty() || y_column.empty())
        {
            throw invalid_argument("--scat requires both an x column and a y column");
        }

        return {x_column, y_column};
    }
}

int main(int argument_count, char *argument_values[])
{
    try
    {
        const ParsedCommandLine command_line =
            parse_command_line(argument_count, argument_values);

        if (has_option(command_line.options, "help"))
        {
            print_help();
            return EXIT_SUCCESS;
        }

        if (command_line.workload_path.empty())
        {
            throw invalid_argument("missing workload filename");
        }

        const bool histogram_mode = has_option(command_line.options, "hist");
        const bool scatter_mode = has_option(command_line.options, "scat");

        if (histogram_mode == scatter_mode)
        {
            throw invalid_argument("choose exactly one plot mode: --hist or --scat");
        }

        const filesystem::path output_path =
            required_option(command_line.options, "out");
        const vector<WorkloadRow> workload_rows =
            load_workload_file(command_line.workload_path);

        PlotOptions plot_options;
        plot_options.title = optional_option(command_line.options, "title", "");

        if (has_option(command_line.options, "xlim"))
        {
            plot_options.x_limits =
                parse_limits(required_option(command_line.options, "xlim"), "x-axis limits");
        }
        if (has_option(command_line.options, "ylim"))
        {
            plot_options.y_limits =
                parse_limits(required_option(command_line.options, "ylim"), "y-axis limits");
        }

        const long long parsed_width =
            parse_integer(optional_option(command_line.options, "width", "1000"), "image width");
        const long long parsed_height =
            parse_integer(optional_option(command_line.options, "height", "700"), "image height");

        if (parsed_width < 0 || parsed_height < 0)
        {
            throw invalid_argument("image width and height cannot be negative");
        }

        plot_options.image_width = static_cast<size_t>(parsed_width);
        plot_options.image_height = static_cast<size_t>(parsed_height);

        if (histogram_mode)
        {
            const string column_name = required_option(command_line.options, "hist");
            const long long parsed_bin_count =
                parse_integer(optional_option(command_line.options, "bins", "10"),
                              "histogram bin count");

            if (parsed_bin_count <= 0)
            {
                throw invalid_argument("histogram bin count must be positive");
            }

            write_histogram_png(output_path,
                                numeric_column(workload_rows, column_name),
                                static_cast<size_t>(parsed_bin_count),
                                column_name,
                                plot_options);
        }
        else
        {
            const auto [x_column_name, y_column_name] =
                parse_scatter_columns(required_option(command_line.options, "scat"));

            write_scatter_png(output_path,
                              numeric_column(workload_rows, x_column_name),
                              numeric_column(workload_rows, y_column_name),
                              x_column_name,
                              y_column_name,
                              plot_options);
        }

        cout << "Wrote plot to " << output_path.string() << '\n';
        return EXIT_SUCCESS;
    }
    catch (const exception &error)
    {
        cerr << "workload-visualizer: " << error.what() << '\n';
        cerr << "Run workload-visualizer --help for usage.\n";
        return EXIT_FAILURE;
    }
}
