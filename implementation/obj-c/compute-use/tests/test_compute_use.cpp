#include "compute_use.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

using namespace std;
using namespace compute_use;

namespace
{
    filesystem::path make_temp_path(const string &name)
    {
        return filesystem::temp_directory_path() / ("compute-use-test-" + name);
    }

    void write_text_file(const filesystem::path &path, const string &contents)
    {
        ofstream output(path, ios::trunc);
        assert(output);
        output << contents;
    }

    string read_text_file(const filesystem::path &path)
    {
        ifstream input(path);
        assert(input);
        return string(istreambuf_iterator<char>(input), istreambuf_iterator<char>());
    }

    void expect_failure(const filesystem::path &input_path, const filesystem::path &output_path)
    {
        bool saw_error = false;
        try
        {
            compute_use_metrics(input_path, output_path);
        }
        catch (const exception &)
        {
            saw_error = true;
        }
        assert(saw_error);
    }
} // namespace

int main()
{
    const filesystem::path input_path = make_temp_path("input.csv");
    const filesystem::path output_path = make_temp_path("output.csv");

    // Fixture metrics: busy avg / queue avg (rounded) / error sum.
    write_text_file(input_path,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,1,1,0\n"
                    "\n"
                    "0,1,1,1,0\n"
                    "1,0,1,1,0\n"
                    "1,1,2,1,0\n"
                    "2,0,3,1,0\n"
                    "2,1,3,1,1\n");

    compute_use_metrics(input_path, output_path);
    const string metrics = read_text_file(output_path);
    assert(metrics.find("time-step,Utilization(10),Saturation(10),Errors(10),"
                         "Utilization(100),Saturation(100),Errors(100),"
                         "Utilization(1000),Saturation(1000),Errors(1000)") != string::npos);
    assert(metrics.find("0,1,1,0,1,1,0,1,1,0") != string::npos);
    assert(metrics.find("1,1,1,0,1,1,0,1,1,0") != string::npos);
    assert(metrics.find("2,1,2,0,1,2,0,1,2,0") != string::npos);
    assert(metrics.find("2,1,2,1,1,2,1,1,2,1") != string::npos);

    // Force an unwritable output path by using an existing directory.
    expect_failure(input_path, filesystem::temp_directory_path());

    // Missing input file.
    expect_failure(make_temp_path("missing-input.csv"), output_path);

    // Empty input file.
    const filesystem::path empty_input = make_temp_path("empty.csv");
    write_text_file(empty_input, "");
    expect_failure(empty_input, output_path);

    // Missing required column.
    const filesystem::path missing_column = make_temp_path("missing-column.csv");
    write_text_file(missing_column,
                    "time-step,queue-sys-id,num-jobs-in-queue,Num-of-errors\n"
                    "0,0,1,0\n");
    expect_failure(missing_column, output_path);

    // Row with too few columns.
    const filesystem::path short_row = make_temp_path("short-row.csv");
    write_text_file(short_row,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,1\n");
    expect_failure(short_row, output_path);

    // Empty numeric field.
    const filesystem::path empty_field = make_temp_path("empty-field.csv");
    write_text_file(empty_field,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,,1,0\n");
    expect_failure(empty_field, output_path);

    // Non-integer numeric field.
    const filesystem::path bad_integer = make_temp_path("bad-integer.csv");
    write_text_file(bad_integer,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,abc,1,0\n");
    expect_failure(bad_integer, output_path);

    // Partial numeric parse and negative values.
    const filesystem::path partial_integer = make_temp_path("partial-integer.csv");
    write_text_file(partial_integer,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,12x,1,0\n");
    expect_failure(partial_integer, output_path);

    const filesystem::path negative_value = make_temp_path("negative.csv");
    write_text_file(negative_value,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,-1,1,0\n");
    expect_failure(negative_value, output_path);

    // Cover CRLF stripping and sliding-window eviction (more than 10 samples).
    const filesystem::path crlf_input = make_temp_path("crlf.csv");
    const filesystem::path crlf_output = make_temp_path("crlf-out.csv");
    string long_input =
        "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\r\n";
    for (int time_step = 0; time_step < 12; ++time_step)
    {
        // Alternate busy/idle; one burst of errors at step 0 leaves the window later.
        const int jobs_in_service = (time_step % 2 == 0) ? 1 : 0;
        const int jobs_in_queue = time_step;
        const int errors = (time_step == 0) ? 5 : 0;
        long_input += to_string(time_step) + ",0," + to_string(jobs_in_queue) + "," +
                      to_string(jobs_in_service) + "," + to_string(errors) + "\r\n";
    }
    write_text_file(crlf_input, long_input);
    compute_use_metrics(crlf_input, crlf_output);
    const string crlf_metrics = read_text_file(crlf_output);
    assert(crlf_metrics.find('\r') == string::npos);
    // Window 10 at step 11 covers samples 2..11:
    // busy avg 0.5 -> 1, queue avg 6.5 -> 7, errors sum 0.
    assert(crlf_metrics.find("11,1,7,0,") != string::npos);

    // Trailing comma restores an empty final field during CSV split.
    const filesystem::path trailing_comma = make_temp_path("trailing-comma.csv");
    const filesystem::path trailing_comma_output = make_temp_path("trailing-comma-out.csv");
    write_text_file(trailing_comma,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors,\n"
                    "0,0,1,1,0,\n");
    compute_use_metrics(trailing_comma, trailing_comma_output);
    const string trailing_metrics = read_text_file(trailing_comma_output);
    assert(trailing_metrics.find("0,1,1,0,1,1,0,1,1,0") != string::npos);

    filesystem::remove(input_path);
    filesystem::remove(output_path);
    filesystem::remove(empty_input);
    filesystem::remove(missing_column);
    filesystem::remove(short_row);
    filesystem::remove(empty_field);
    filesystem::remove(bad_integer);
    filesystem::remove(partial_integer);
    filesystem::remove(negative_value);
    filesystem::remove(crlf_input);
    filesystem::remove(crlf_output);
    filesystem::remove(trailing_comma);
    filesystem::remove(trailing_comma_output);

    return 0;
}
