#include "filter_by_job.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

using namespace std;
using namespace filter_by_job;

namespace
{
    filesystem::path make_temp_path(const string &name)
    {
        return filesystem::temp_directory_path() / ("filter-by-job-test-" + name);
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
    const filesystem::path input_path = make_temp_path("input.log");
    const filesystem::path output_path = make_temp_path("output.log");

    write_text_file(input_path,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "\n"
                    "10 j1 IN q0 ARRIVE-VIA in0.wl\n"
                    "20 j0 IN q0 EXITS-SERVER 0\n");

    unordered_set<string> allowed_jobs = {"j0"};
    filter_event_log(input_path, output_path, allowed_jobs);

    const string filtered = read_text_file(output_path);
    assert(filtered.find("Event Log") != string::npos);
    assert(filtered.find("0 j0 IN q0 ARRIVE-VIA in0.wl") != string::npos);
    assert(filtered.find("20 j0 IN q0 EXITS-SERVER 0") != string::npos);
    assert(filtered.find("10 j1 IN q0 ARRIVE-VIA in0.wl") == string::npos);

    // Force an unwritable output path by using an existing directory.
    bool saw_bad_output = false;
    try
    {
        filter_event_log(input_path, filesystem::temp_directory_path(), allowed_jobs);
    }
    catch (const exception &)
    {
        saw_bad_output = true;
    }
    assert(saw_bad_output);

    bool saw_empty_jobs = false;
    try
    {
        filter_event_log(input_path, output_path, {});
    }
    catch (const exception &)
    {
        saw_empty_jobs = true;
    }
    assert(saw_empty_jobs);

    bool saw_missing_input = false;
    try
    {
        filter_event_log(make_temp_path("missing-input.log"), output_path, allowed_jobs);
    }
    catch (const exception &)
    {
        saw_missing_input = true;
    }
    assert(saw_missing_input);

    const filesystem::path empty_input = make_temp_path("empty.log");
    write_text_file(empty_input, "");
    bool saw_empty_file = false;
    try
    {
        filter_event_log(empty_input, output_path, allowed_jobs);
    }
    catch (const exception &)
    {
        saw_empty_file = true;
    }
    assert(saw_empty_file);

    const filesystem::path short_row = make_temp_path("short-row.log");
    write_text_file(short_row, "Event Log\n0\n");
    bool saw_short_row = false;
    try
    {
        filter_event_log(short_row, output_path, allowed_jobs);
    }
    catch (const exception &)
    {
        saw_short_row = true;
    }
    assert(saw_short_row);

    // Cover CRLF stripping (getline leaves a trailing '\r').
    const filesystem::path crlf_input = make_temp_path("crlf.log");
    const filesystem::path crlf_output = make_temp_path("crlf-out.log");
    write_text_file(crlf_input,
                    "Event Log\r\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\r\n"
                    "10 j1 IN q0 ARRIVE-VIA in0.wl\r\n");
    filter_event_log(crlf_input, crlf_output, allowed_jobs);
    const string crlf_filtered = read_text_file(crlf_output);
    assert(crlf_filtered.find("0 j0 IN q0 ARRIVE-VIA in0.wl") != string::npos);
    assert(crlf_filtered.find("10 j1 IN q0 ARRIVE-VIA in0.wl") == string::npos);
    assert(crlf_filtered.find('\r') == string::npos);

    filesystem::remove(input_path);
    filesystem::remove(output_path);
    filesystem::remove(empty_input);
    filesystem::remove(short_row);
    filesystem::remove(crlf_input);
    filesystem::remove(crlf_output);

    return 0;
}
