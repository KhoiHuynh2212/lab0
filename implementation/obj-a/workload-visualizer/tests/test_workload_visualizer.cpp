#include "workload_visualizer.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace std;
using namespace workload_visualizer;

namespace
{
    int checks_run = 0;

    void require(bool condition, const string &message)
    {
        ++checks_run;
        if (!condition)
        {
            throw runtime_error("test failed: " + message);
        }
    }

    template <typename Callable>
    void require_throws(Callable callable, const string &message)
    {
        ++checks_run;
        try
        {
            callable();
        }
        catch (const exception &)
        {
            return;
        }

        throw runtime_error("test failed: expected exception: " + message);
    }

    vector<uint8_t> read_binary_file(const filesystem::path &path)
    {
        ifstream input_file(path, ios::binary);
        if (!input_file)
        {
            throw runtime_error("could not read test file: " + path.string());
        }

        return vector<uint8_t>((istreambuf_iterator<char>(input_file)),
                               istreambuf_iterator<char>());
    }

    uint32_t read_uint32_big_endian(const vector<uint8_t> &bytes, size_t offset)
    {
        return (static_cast<uint32_t>(bytes.at(offset)) << 24U) |
               (static_cast<uint32_t>(bytes.at(offset + 1)) << 16U) |
               (static_cast<uint32_t>(bytes.at(offset + 2)) << 8U) |
               static_cast<uint32_t>(bytes.at(offset + 3));
    }
}

int main()
{
    try
    {
        const vector<string> parsed_csv =
            parse_csv_line("j0,0,\"['0: W 10', '1: W 20']\"");
        require(parsed_csv.size() == 3, "CSV row has three fields");
        require(parsed_csv[0] == "j0", "CSV Job Id field");
        require(parsed_csv[2] == "['0: W 10', '1: W 20']", "quoted Tasks field");

        const vector<string> escaped_quote_csv =
            parse_csv_line("\"job \"\"quoted\"\"\",4,[]");
        require(escaped_quote_csv[0] == "job \"quoted\"", "escaped CSV quotes");
        require_throws([]
                       { parse_csv_line("\"unterminated"); },
                       "unterminated CSV quote");

        const filesystem::path temporary_directory =
            filesystem::temp_directory_path() / "obj_a_workload_visualizer_tests";
        filesystem::remove_all(temporary_directory);
        filesystem::create_directories(temporary_directory);

        const filesystem::path workload_path = temporary_directory / "workload.csv";
        {
            ofstream workload_file(workload_path);
            workload_file << "Job Id,arrival time,Tasks\n";
            workload_file << "j0,0,\"['0: W 10', '1: W 10']\"\n";
            workload_file << "j1,10,\"['0: W 10']\"\n";
            workload_file << "j2,20,\"['0: W 10', '1: W 10', '2: W 10']\"\n";
            workload_file << "j3,30,[]\n";
        }

        const vector<WorkloadRow> workload_rows = load_workload_file(workload_path);
        require(workload_rows.size() == 4, "four workload rows are loaded");
        require(workload_rows[0] == WorkloadRow{"j0", 0, 2}, "first workload row");
        require(workload_rows[2] == WorkloadRow{"j2", 20, 3}, "three-task workload row");
        require(workload_rows[3].task_count == 0, "empty task list has length zero");

        const vector<double> arrival_times = numeric_column(workload_rows, "arrival time");
        const vector<double> task_counts = numeric_column(workload_rows, "len(Tasks)");
        require(arrival_times == vector<double>({0.0, 10.0, 20.0, 30.0}),
                "arrival-time numeric column");
        require(task_counts == vector<double>({2.0, 1.0, 3.0, 0.0}),
                "len(Tasks) numeric column");
        require(numeric_column(workload_rows, "task count") == task_counts,
                "task count alias");
        require_throws([&]
                       { numeric_column(workload_rows, "Job Id"); },
                       "string Job Id cannot be plotted as a numeric column");

        PlotOptions histogram_options;
        histogram_options.title = "Arrival Time Histogram";
        histogram_options.image_width = 640;
        histogram_options.image_height = 480;
        histogram_options.x_limits = pair<double, double>{0.0, 30.0};
        histogram_options.y_limits = pair<double, double>{0.0, 3.0};

        const filesystem::path histogram_path = temporary_directory / "histogram.png";
        write_histogram_png(histogram_path,
                            arrival_times,
                            3,
                            "arrival time",
                            histogram_options);

        const vector<uint8_t> histogram_bytes = read_binary_file(histogram_path);
        const array<uint8_t, 8> png_signature = {137, 80, 78, 71, 13, 10, 26, 10};
        require(histogram_bytes.size() > 100, "histogram PNG has content");
        require(equal(png_signature.begin(), png_signature.end(), histogram_bytes.begin()),
                "histogram PNG signature");
        require(read_uint32_big_endian(histogram_bytes, 16) == 640,
                "histogram PNG width");
        require(read_uint32_big_endian(histogram_bytes, 20) == 480,
                "histogram PNG height");

        PlotOptions scatter_options;
        scatter_options.title = "Arrival vs Tasks";
        scatter_options.image_width = 800;
        scatter_options.image_height = 500;

        const filesystem::path scatter_path = temporary_directory / "scatter.png";
        write_scatter_png(scatter_path,
                          arrival_times,
                          task_counts,
                          "arrival time",
                          "len(Tasks)",
                          scatter_options);

        const vector<uint8_t> scatter_bytes = read_binary_file(scatter_path);
        require(scatter_bytes.size() > 100, "scatter PNG has content");
        require(equal(png_signature.begin(), png_signature.end(), scatter_bytes.begin()),
                "scatter PNG signature");
        require(read_uint32_big_endian(scatter_bytes, 16) == 800,
                "scatter PNG width");
        require(read_uint32_big_endian(scatter_bytes, 20) == 500,
                "scatter PNG height");

        require_throws([&]
                       { write_histogram_png(temporary_directory / "bad.jpg",
                                             arrival_times,
                                             3,
                                             "arrival time",
                                             histogram_options); },
                       "output must be PNG");
        require_throws([&]
                       { write_histogram_png(temporary_directory / "bad-bins.png",
                                             arrival_times,
                                             0,
                                             "arrival time",
                                             histogram_options); },
                       "zero histogram bins");
        require_throws([&]
                       { write_histogram_png(temporary_directory / "empty.png",
                                             {},
                                             5,
                                             "arrival time",
                                             histogram_options); },
                       "empty histogram data");
        require_throws([&]
                       { write_scatter_png(temporary_directory / "mismatch.png",
                                           {1.0, 2.0},
                                           {1.0},
                                           "x",
                                           "y",
                                           scatter_options); },
                       "scatter column lengths must match");
        require_throws([&]
                       {
                           PlotOptions bad_size;
                           bad_size.image_width = 100;
                           write_scatter_png(temporary_directory / "small.png",
                                             {1.0},
                                             {1.0},
                                             "x",
                                             "y",
                                             bad_size); },
                       "minimum image dimensions");
        require_throws([&]
                       {
                           PlotOptions bad_limits;
                           bad_limits.x_limits = pair<double, double>{5.0, 5.0};
                           write_scatter_png(temporary_directory / "limits.png",
                                             {1.0},
                                             {1.0},
                                             "x",
                                             "y",
                                             bad_limits); },
                       "x-axis max must exceed min");

        const filesystem::path bad_workload_path = temporary_directory / "bad-workload.csv";
        {
            ofstream bad_workload_file(bad_workload_path);
            bad_workload_file << "Job Id,arrival time\n";
            bad_workload_file << "j0,0\n";
        }
        require_throws([&]
                       { load_workload_file(bad_workload_path); },
                       "missing Tasks column");

        filesystem::remove_all(temporary_directory);

        cout << "PASS: " << checks_run << " workload-visualizer checks\n";
        return 0;
    }
    catch (const exception &error)
    {
        cerr << error.what() << '\n';
        return 1;
    }
}
