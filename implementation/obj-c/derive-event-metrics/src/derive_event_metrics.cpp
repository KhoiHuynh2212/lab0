#include "derive_event_metrics.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

using namespace std;

namespace derive_event_metrics
{
    namespace
    {
        string visit_key(const string &job_id, const string &queue_id)
        {
            return job_id + "|" + queue_id;
        }

        // -1 means the visit never got there, so leave the cell empty.
        void write_field(ofstream &output, long value)
        {
            if (value >= 0)
            {
                output << value;
            }
        }
    } // namespace

    vector<JobVisit> derive_visits(const filesystem::path &input_path)
    {
        ifstream input(input_path);
        if (!input)
        {
            throw runtime_error("cannot read input file: " + input_path.string());
        }

        vector<JobVisit> finished;
        map<string, JobVisit> open_visits;
        vector<string> open_order;

        string line;
        while (getline(input, line))
        {
            istringstream fields(line);
            long time = 0;
            string job_id;
            string in_word;
            string queue_id;
            string event;

            // <time> <job> IN <queue> <EVENT> ...
            if (!(fields >> time >> job_id >> in_word >> queue_id >> event) || in_word != "IN")
            {
                continue;
            }

            const string key = visit_key(job_id, queue_id);

            if (event == "ARRIVE-VIA")
            {
                JobVisit visit;
                visit.job_id = job_id;
                visit.queue_id = queue_id;
                visit.arrival_time = time;
                open_visits[key] = visit;
                open_order.push_back(key);
                continue;
            }

            auto found = open_visits.find(key);
            if (found == open_visits.end())
            {
                continue;
            }
            JobVisit &visit = found->second;

            if (event == "ENTERS-SERVER")
            {
                if (visit.first_enter_time < 0)
                {
                    visit.first_enter_time = time;
                }
                visit.current_enter_time = time;
            }
            else if (event == "EXITS-SERVER")
            {
                if (visit.current_enter_time >= 0)
                {
                    visit.time_in_service += time - visit.current_enter_time;
                    visit.current_enter_time = -1;
                }
            }
            else if (event == "DEPART-VIA" || event == "ERROR")
            {
                visit.depart_time = time;
                finished.push_back(visit);
                open_visits.erase(found);
            }
        }

        // Visits still open when the log ended.
        for (const string &key : open_order)
        {
            auto still_open = open_visits.find(key);
            if (still_open != open_visits.end())
            {
                finished.push_back(still_open->second);
            }
        }

        return finished;
    }

    void write_metrics(const filesystem::path &output_path,
                       const vector<JobVisit> &visits)
    {
        ofstream output(output_path, ios::trunc);
        if (!output)
        {
            throw runtime_error("cannot write output file: " + output_path.string());
        }

        output << "job-id,queue-sys-id,arrival-time,time-in-queue,"
                  "time-in-service,response-time,turn-around-time\n";

        for (const JobVisit &visit : visits)
        {
            long response_time = -1;
            if (visit.first_enter_time >= 0)
            {
                response_time = visit.first_enter_time - visit.arrival_time;
            }

            long turn_around_time = -1;
            if (visit.depart_time >= 0)
            {
                turn_around_time = visit.depart_time - visit.arrival_time;
            }

            long time_in_queue = -1;
            if (turn_around_time >= 0)
            {
                time_in_queue = turn_around_time - visit.time_in_service;
            }
            else if (response_time >= 0)
            {
                time_in_queue = response_time;
            }

            output << visit.job_id << ','
                   << visit.queue_id << ','
                   << visit.arrival_time << ',';
            write_field(output, time_in_queue);
            output << ',' << visit.time_in_service << ',';
            write_field(output, response_time);
            output << ',';
            write_field(output, turn_around_time);
            output << '\n';
        }
    }

}
