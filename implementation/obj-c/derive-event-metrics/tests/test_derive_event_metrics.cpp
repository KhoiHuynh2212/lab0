#include "derive_event_metrics.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace derive_event_metrics;

namespace
{
    filesystem::path make_temp_path(const string &name)
    {
        return filesystem::temp_directory_path() / ("derive-event-metrics-test-" + name);
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

    // j0 served right away, j1 waits, j2 dropped, j3 still running at the end.
    // Header, blank, junk and unknown-job (j9) lines must be skipped.
    write_text_file(input_path,
                    "Event Log\n"
                    "0 j0 IN q0 ARRIVE-VIA in.wl\n"
                    "0 j0 IN q0 ENTERS-SERVER 0\n"
                    "0 j0 IN q0 TASK-START 0 \"W 10\"\n"
                    "\n"
                    "not an event line\n"
                    "0 j1 IN q0 ARRIVE-VIA in.wl\n"
                    "1 j2 IN q0 ARRIVE-VIA in.wl\n"
                    "1 j2 IN q0 ERROR QUEUE-FULL\n"
                    "2 j0 IN q0 TASK-STOP 0 \"W 10\"\n"
                    "2 j0 IN q0 EXITS-SERVER 0\n"
                    "2 j0 IN q0 DEPART-VIA out.wl\n"
                    "2 j1 IN q0 ENTERS-SERVER 0\n"
                    "4 j1 IN q0 EXITS-SERVER 0\n"
                    "4 j1 IN q0 DEPART-VIA out.wl\n"
                    "5 j9 IN q0 EXITS-SERVER 0\n"
                    "6 j3 IN q0 ARRIVE-VIA in.wl\n"
                    "6 j3 IN q0 ENTERS-SERVER 0\n");

    const vector<JobVisit> visits = derive_visits(input_path);
    assert(visits.size() == 4);

    assert(visits[0].job_id == "j2");
    assert(visits[0].first_enter_time == -1);
    assert(visits[0].depart_time == 1);

    assert(visits[1].job_id == "j0");
    assert(visits[1].arrival_time == 0);
    assert(visits[1].first_enter_time == 0);
    assert(visits[1].time_in_service == 2);
    assert(visits[1].depart_time == 2);

    assert(visits[2].job_id == "j1");
    assert(visits[2].first_enter_time == 2);
    assert(visits[2].time_in_service == 2);
    assert(visits[2].depart_time == 4);

    assert(visits[3].job_id == "j3");
    assert(visits[3].depart_time == -1);

    write_metrics(output_path, visits);
    const string csv = read_text_file(output_path);
    assert(csv.find("job-id,queue-sys-id,arrival-time,time-in-queue,"
                    "time-in-service,response-time,turn-around-time\n") == 0);
    assert(csv.find("j2,q0,1,0,0,,0\n") != string::npos);
    assert(csv.find("j0,q0,0,0,2,0,2\n") != string::npos);
    assert(csv.find("j1,q0,0,2,2,2,4\n") != string::npos);
    assert(csv.find("j3,q0,6,0,0,0,\n") != string::npos);

    bool saw_bad_input = false;
    try
    {
        derive_visits(make_temp_path("does-not-exist.log"));
    }
    catch (const runtime_error &)
    {
        saw_bad_input = true;
    }
    assert(saw_bad_input);

    bool saw_bad_output = false;
    try
    {
        write_metrics(filesystem::temp_directory_path(), visits);
    }
    catch (const runtime_error &)
    {
        saw_bad_output = true;
    }
    assert(saw_bad_output);

    filesystem::remove(input_path);
    filesystem::remove(output_path);
    return 0;
}
