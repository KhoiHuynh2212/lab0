#include "derive_sys_metrics.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace std;
using namespace derive_sys_metrics;

namespace
{
    filesystem::path make_temp_path(const string &name)
    {
        return filesystem::temp_directory_path() / ("derive-sys-metrics-test-" + name);
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
    const filesystem::path output_path = make_temp_path("output.csv");

    write_text_file(input_path,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "\n"
                    "10 j1 IN q0 ARRIVE-VIA in0.wl\n"
                    "20 j0 IN q0 DEPART-VIA out0.wl\n"
                    "30 j1 IN q0 DEPART-VIA out0.wl\n");

    derive_system_metrics(input_path, output_path);

    const string metrics = read_text_file(output_path);
    assert(metrics.find("time-step,Avg-response-time,median-response-time,"
                        "10-response-time,90-response-time,throughput") != string::npos);
    assert(metrics.find("20,20,20,20,20,1") != string::npos);
    assert(metrics.find("30,20,20,20,20,1") != string::npos);

    // Multiple exits at one time-step exercise average / median / percentiles.
    write_text_file(input_path,
                    "Event Log\n"
                    "0 a IN q0 ARRIVE-VIA in.wl\n"
                    "5 b IN q0 ARRIVE-VIA in.wl\n"
                    "10 c IN q0 ARRIVE-VIA in.wl\n"
                    "15 d IN q0 ARRIVE-VIA in.wl\n"
                    "20 e IN q0 ARRIVE-VIA in.wl\n"
                    "100 a IN q0 DEPART-VIA out.wl\n"
                    "100 b IN q0 DEPART-VIA out.wl\n"
                    "100 c IN q0 DEPART-VIA out.wl\n"
                    "100 d IN q0 DEPART-VIA out.wl\n"
                    "100 e IN q0 DEPART-VIA out.wl\n");
    derive_system_metrics(input_path, output_path);
    const string multi_exit = read_text_file(output_path);
    // RTs sorted: 80,85,90,95,100 → avg 90, median 90, p10=80, p90=100
    assert(multi_exit.find("100,90,90,80,100,5") != string::npos);

    // Edge hops (non-.wl) must be ignored for system metrics.
    write_text_file(input_path,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "10 j0 IN q1 ARRIVE-VIA hop0\n"
                    "20 j0 IN q1 DEPART-VIA hop1\n"
                    "30 j0 IN q2 DEPART-VIA out0.wl\n");
    derive_system_metrics(input_path, output_path);
    const string edge_ignored = read_text_file(output_path);
    assert(edge_ignored.find("30,30,30,30,30,1") != string::npos);
    assert(edge_ignored.find("\n20,") == string::npos);

    // Even-count median uses rounded average of the two middle values.
    write_text_file(input_path,
                    "Event Log\n"
                    "0 a IN q ARRIVE-VIA in.wl\n"
                    "2 b IN q ARRIVE-VIA in.wl\n"
                    "10 a IN q DEPART-VIA out.wl\n"
                    "10 b IN q DEPART-VIA out.wl\n");
    derive_system_metrics(input_path, output_path);
    const string even_same_exit = read_text_file(output_path);
    // RTs 10 and 8 → sorted 8,10 → avg 9, median 9, p10=8, p90=10
    assert(even_same_exit.find("10,9,9,8,10,2") != string::npos);

    // Second source arrival for the same job is ignored (first arrival wins).
    write_text_file(input_path,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "5 j0 IN q0 ARRIVE-VIA in1.wl\n"
                    "20 j0 IN q0 DEPART-VIA out0.wl\n");
    derive_system_metrics(input_path, output_path);
    const string first_arrival_wins = read_text_file(output_path);
    assert(first_arrival_wins.find("20,20,20,20,20,1") != string::npos);

    bool saw_bad_output = false;
    try
    {
        derive_system_metrics(input_path, filesystem::temp_directory_path());
    }
    catch (const exception &)
    {
        saw_bad_output = true;
    }
    assert(saw_bad_output);

    bool saw_missing_input = false;
    try
    {
        derive_system_metrics(make_temp_path("missing-input.log"), output_path);
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
        derive_system_metrics(empty_input, output_path);
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
        derive_system_metrics(short_row, output_path);
    }
    catch (const exception &)
    {
        saw_short_row = true;
    }
    assert(saw_short_row);

    const filesystem::path missing_arrive = make_temp_path("missing-arrive.log");
    write_text_file(missing_arrive,
                    "Event Log\n"
                    "10 j0 IN q0 DEPART-VIA out0.wl\n");
    bool saw_missing_arrive = false;
    try
    {
        derive_system_metrics(missing_arrive, output_path);
    }
    catch (const exception &)
    {
        saw_missing_arrive = true;
    }
    assert(saw_missing_arrive);

    const filesystem::path missing_via = make_temp_path("missing-via.log");
    write_text_file(missing_via,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA\n");
    bool saw_missing_via = false;
    try
    {
        derive_system_metrics(missing_via, output_path);
    }
    catch (const exception &)
    {
        saw_missing_via = true;
    }
    assert(saw_missing_via);

    const filesystem::path bad_time = make_temp_path("bad-time.log");
    write_text_file(bad_time,
                    "Event Log\n"
                    "x j0 IN q0 ARRIVE-VIA in0.wl\n");
    bool saw_bad_time = false;
    try
    {
        derive_system_metrics(bad_time, output_path);
    }
    catch (const exception &)
    {
        saw_bad_time = true;
    }
    assert(saw_bad_time);

    const filesystem::path partial_time = make_temp_path("partial-time.log");
    write_text_file(partial_time,
                    "Event Log\n"
                    "10x j0 IN q0 ARRIVE-VIA in0.wl\n");
    bool saw_partial_time = false;
    try
    {
        derive_system_metrics(partial_time, output_path);
    }
    catch (const exception &)
    {
        saw_partial_time = true;
    }
    assert(saw_partial_time);

    const filesystem::path negative_time = make_temp_path("negative-time.log");
    write_text_file(negative_time,
                    "Event Log\n"
                    "-1 j0 IN q0 ARRIVE-VIA in0.wl\n");
    bool saw_negative_time = false;
    try
    {
        derive_system_metrics(negative_time, output_path);
    }
    catch (const exception &)
    {
        saw_negative_time = true;
    }
    assert(saw_negative_time);

    const filesystem::path missing_depart_via = make_temp_path("missing-depart-via.log");
    write_text_file(missing_depart_via,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "10 j0 IN q0 DEPART-VIA\n");
    bool saw_missing_depart_via = false;
    try
    {
        derive_system_metrics(missing_depart_via, output_path);
    }
    catch (const exception &)
    {
        saw_missing_depart_via = true;
    }
    assert(saw_missing_depart_via);

    const filesystem::path negative_response = make_temp_path("negative-response.log");
    write_text_file(negative_response,
                    "Event Log\n"
                    "20 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "10 j0 IN q0 DEPART-VIA out0.wl\n");
    bool saw_negative_response = false;
    try
    {
        derive_system_metrics(negative_response, output_path);
    }
    catch (const exception &)
    {
        saw_negative_response = true;
    }
    assert(saw_negative_response);

    // Cover CRLF stripping.
    const filesystem::path crlf_input = make_temp_path("crlf.log");
    const filesystem::path crlf_output = make_temp_path("crlf-out.csv");
    write_text_file(crlf_input,
                    "Event Log\r\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\r\n"
                    "20 j0 IN q0 DEPART-VIA out0.wl\r\n");
    derive_system_metrics(crlf_input, crlf_output);
    const string crlf_metrics = read_text_file(crlf_output);
    assert(crlf_metrics.find("20,20,20,20,20,1") != string::npos);
    assert(crlf_metrics.find('\r') == string::npos);

    // No sink departures → header only.
    write_text_file(input_path,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in0.wl\n"
                    "10 j0 IN q0 ENTERS-SERVER 0\n");
    derive_system_metrics(input_path, output_path);
    const string header_only = read_text_file(output_path);
    assert(header_only ==
           "time-step,Avg-response-time,median-response-time,"
           "10-response-time,90-response-time,throughput\n");

    filesystem::remove(input_path);
    filesystem::remove(output_path);
    filesystem::remove(empty_input);
    filesystem::remove(short_row);
    filesystem::remove(missing_arrive);
    filesystem::remove(missing_via);
    filesystem::remove(bad_time);
    filesystem::remove(partial_time);
    filesystem::remove(negative_time);
    filesystem::remove(missing_depart_via);
    filesystem::remove(negative_response);
    filesystem::remove(crlf_input);
    filesystem::remove(crlf_output);

    return 0;
}
