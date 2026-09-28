#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace workload_visualizer
{

    // Only the values needed for Obj-A plots are kept after reading the workload.
    struct WorkloadRow
    {
        string job_id;
        long long arrival_time = 0;
        size_t task_count = 0;

        bool operator==(const WorkloadRow &) const = default;
    };

    struct PlotOptions
    {
        optional<pair<double, double>> x_limits;
        optional<pair<double, double>> y_limits;
        string title;
        size_t image_width = 1000;
        size_t image_height = 700;
    };

    // Parse a CSV row while respecting quoted fields and escaped quotes.
    vector<string> parse_csv_line(const string &csv_line);

    // Read Job Id, arrival time, and Tasks from a workload file.
    vector<WorkloadRow> load_workload_file(const filesystem::path &workload_path);

    // Return the numeric values represented by a supported workload column.
    // Supported columns include "arrival time" and "len(Tasks)".
    vector<double> numeric_column(const vector<WorkloadRow> &workload_rows,
                                  const string &column_name);

    // Create a PNG histogram for one workload column.
    void write_histogram_png(const filesystem::path &output_path,
                             const vector<double> &values,
                             size_t bin_count,
                             const string &x_axis_label,
                             const PlotOptions &plot_options);

    // Create a PNG scatter plot for two workload columns.
    void write_scatter_png(const filesystem::path &output_path,
                           const vector<double> &x_values,
                           const vector<double> &y_values,
                           const string &x_axis_label,
                           const string &y_axis_label,
                           const PlotOptions &plot_options);

} // namespace workload_visualizer
