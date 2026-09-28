#include "filter_by_queue.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;
using namespace filter_by_queue;

namespace
{
    filesystem::path make_temp_path(const string &name)
    {
        return filesystem::temp_directory_path() / ("filter-by-queue-test-" + name);
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
} // namespace

int main()
{
    const filesystem::path input_path = make_temp_path("input.csv");
    const filesystem::path output_path = make_temp_path("output.csv");

    write_text_file(input_path,
                    "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,Num-of-errors\n"
                    "0,0,1,0,0\n"
                    "\n"
                    "0,1,2,1,0\n"
                    "1,0,0,1,0\n");

    unordered_set<string> allowed_queues = {"0"};
    filter_metrics_file(input_path, output_path, allowed_queues);

    const string filtered = read_text_file(output_path);
    assert(filtered.find("0,0,1,0,0") != string::npos);
    assert(filtered.find("1,0,0,1,0") != string::npos);
    assert(filtered.find("0,1,2,1,0") == string::npos);

    // Force an unwritable output path by using an existing directory.
    bool saw_bad_output = false;
    try
    {
        filter_metrics_file(input_path, filesystem::temp_directory_path(), allowed_queues);
    }
    catch (const exception &)
    {
        saw_bad_output = true;
    }
    assert(saw_bad_output);

    bool saw_empty_queues = false;
    try
    {
        filter_metrics_file(input_path, output_path, {});
    }
    catch (const exception &)
    {
        saw_empty_queues = true;
    }
    assert(saw_empty_queues);

    bool saw_missing_input = false;
    try
    {
        filter_metrics_file(make_temp_path("missing-input.csv"), output_path, allowed_queues);
    }
    catch (const exception &)
    {
        saw_missing_input = true;
    }
    assert(saw_missing_input);

    const filesystem::path empty_input = make_temp_path("empty.csv");
    write_text_file(empty_input, "");
    bool saw_empty_file = false;
    try
    {
        filter_metrics_file(empty_input, output_path, allowed_queues);
    }
    catch (const exception &)
    {
        saw_empty_file = true;
    }
    assert(saw_empty_file);

    const filesystem::path bad_header = make_temp_path("bad-header.csv");
    write_text_file(bad_header, "time-step,other-column\n0,1\n");
    bool saw_missing_column = false;
    try
    {
        filter_metrics_file(bad_header, output_path, allowed_queues);
    }
    catch (const exception &)
    {
        saw_missing_column = true;
    }
    assert(saw_missing_column);

    const filesystem::path short_row = make_temp_path("short-row.csv");
    write_text_file(short_row, "time-step,queue-sys-id\n0\n");
    bool saw_short_row = false;
    try
    {
        filter_metrics_file(short_row, output_path, allowed_queues);
    }
    catch (const exception &)
    {
        saw_short_row = true;
    }
    assert(saw_short_row);

    // Cover trailing-empty CSV fields (line ends with ',').
    const filesystem::path trailing_comma = make_temp_path("trailing-comma.csv");
    const filesystem::path trailing_comma_out = make_temp_path("trailing-comma-out.csv");
    write_text_file(trailing_comma,
                    "time-step,queue-sys-id,extra,\n"
                    "0,0,keep,\n"
                    "0,1,drop,\n");
    filter_metrics_file(trailing_comma, trailing_comma_out, allowed_queues);
    const string trailing_filtered = read_text_file(trailing_comma_out);
    assert(trailing_filtered.find("0,0,keep,") != string::npos);
    assert(trailing_filtered.find("0,1,drop,") == string::npos);

    // Cover CRLF stripping (getline leaves a trailing '\r').
    const filesystem::path crlf_input = make_temp_path("crlf.csv");
    const filesystem::path crlf_output = make_temp_path("crlf-out.csv");
    write_text_file(crlf_input,
                    "time-step,queue-sys-id\r\n"
                    "0,0\r\n"
                    "0,1\r\n");
    filter_metrics_file(crlf_input, crlf_output, allowed_queues);
    const string crlf_filtered = read_text_file(crlf_output);
    assert(crlf_filtered.find("0,0") != string::npos);
    assert(crlf_filtered.find("0,1") == string::npos);
    assert(crlf_filtered.find('\r') == string::npos);

    filesystem::remove(input_path);
    filesystem::remove(output_path);
    filesystem::remove(empty_input);
    filesystem::remove(bad_header);
    filesystem::remove(short_row);
    filesystem::remove(trailing_comma);
    filesystem::remove(trailing_comma_out);
    filesystem::remove(crlf_input);
    filesystem::remove(crlf_output);

    return 0;
}
