#include "multi_queue_sim.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
namespace file_system = filesystem;

namespace
{

    // Small test helpers, mirroring Obj-B's test_queue_sim.cpp conventions.
    int failed_check_count = 0;
    int total_check_count = 0;

#define CHECK(condition)                                                            \
    do                                                                              \
    {                                                                               \
        ++total_check_count;                                                        \
        if (!(condition))                                                           \
        {                                                                           \
            ++failed_check_count;                                                   \
            cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #condition "\n"; \
        }                                                                           \
    } while (0)

#define CHECK_EQ(actual_expression, expected_expression)                                                                \
    do                                                                                                                  \
    {                                                                                                                   \
        const auto evaluated_actual_value = (actual_expression);                                                        \
        const auto evaluated_expected_value = (expected_expression);                                                    \
        ++total_check_count;                                                                                            \
        if (!(evaluated_actual_value == evaluated_expected_value))                                                      \
        {                                                                                                               \
            ++failed_check_count;                                                                                       \
            cerr << "FAIL " << __FILE__ << ':' << __LINE__ << ": " #actual_expression " == " #expected_expression "\n"; \
        }                                                                                                               \
    } while (0)

    template <typename Callable>
    void check_that_exception_is_thrown(Callable &&callable_expected_to_throw)
    {
        ++total_check_count;
        try
        {
            callable_expected_to_throw();
            ++failed_check_count;
            cerr << "FAIL expected exception\n";
        }
        catch (const exception &)
        {
            // The exception means the check passed.
        }
    }

    void write_text_file(const file_system::path &output_path, const string &file_contents)
    {
        if (!output_path.parent_path().empty())
        {
            file_system::create_directories(output_path.parent_path());
        }
        ofstream output_file(output_path);
        if (!output_file)
        {
            throw runtime_error("test could not write " + output_path.string());
        }
        output_file << file_contents;
    }

    string read_text_file(const file_system::path &input_path)
    {
        ifstream input_file(input_path);
        ostringstream complete_contents;
        complete_contents << input_file.rdbuf();
        return complete_contents.str();
    }

    size_t count_newline_characters(const string &text)
    {
        size_t count = 0;
        for (char c : text)
        {
            if (c == '\n')
            {
                ++count;
            }
        }
        return count;
    }

    // Count how many times a substring occurs (used for round-robin checks).
    size_t count_occurrences(const string &haystack, const string &needle)
    {
        size_t count = 0;
        size_t position = 0;
        while ((position = haystack.find(needle, position)) != string::npos)
        {
            ++count;
            position += needle.size();
        }
        return count;
    }

    file_system::path create_clean_test_case_directory(const file_system::path &test_root_directory,
                                                       const string &test_case_name)
    {
        const file_system::path test_case_directory = test_root_directory / test_case_name;
        file_system::remove_all(test_case_directory);
        file_system::create_directories(test_case_directory);
        return test_case_directory;
    }

    // ------------------------------------------------------------------
    // load_multi_queue_config: parsing and validation
    // ------------------------------------------------------------------
    void test_load_multi_queue_config(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "load-config");

        // A single row behaves just like Obj-B's one-row configuration.
        write_text_file(
            test_case_directory / "single.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,2,\"Sub-5,Sub-10\",3,in.wl,out.wl,[],[]\n");

        const mqsim::MultiQueueConfig single_row_config =
            mqsim::load_multi_queue_config(test_case_directory / "single.csv");
        CHECK_EQ(single_row_config.queue_systems.size(), 1U);
        CHECK_EQ(single_row_config.queue_systems.at(0).queue_system_id, "q0");
        CHECK_EQ(single_row_config.queue_systems.at(0).maximum_queue_size, 8U);
        CHECK_EQ(single_row_config.queue_systems.at(0).number_of_servers, 2U);

        // Multiple rows, with edges naming other rows in the file.
        write_text_file(
            test_case_directory / "chain.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,1,Sub-10,10,in0.wl,,[],[q1]\n"
            "q1,8,1,Sub-10,10,,out1.wl,[q0],[]\n");

        const mqsim::MultiQueueConfig chain_config =
            mqsim::load_multi_queue_config(test_case_directory / "chain.csv");
        CHECK_EQ(chain_config.queue_systems.size(), 2U);
        CHECK_EQ(chain_config.queue_systems.at(0).outgoing_edges.at(0), "q1");
        CHECK_EQ(chain_config.queue_systems.at(1).incoming_edges.at(0), "q0");

        // Edges that use ';' or bare (no bracket) lists also parse correctly.
        write_text_file(
            test_case_directory / "semicolon.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "a,1,1,Sub-1,1,,,,b;c\n"
            "b,1,1,Sub-1,1,,,a,\n"
            "c,1,1,Sub-1,1,,,a,\n");
        const mqsim::MultiQueueConfig semicolon_config =
            mqsim::load_multi_queue_config(test_case_directory / "semicolon.csv");
        CHECK_EQ(semicolon_config.queue_systems.at(0).outgoing_edges.size(), 2U);

        // Empty file / header-only file / missing file are all invalid.
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "missing.csv"); });

        write_text_file(test_case_directory / "empty.csv", "");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "empty.csv"); });

        write_text_file(
            test_case_directory / "header-only.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "header-only.csv"); });

        // Duplicate Queue sys ID.
        write_text_file(
            test_case_directory / "dup.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,,,[],[]\n"
            "q0,1,1,Sub-1,1,,,[],[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "dup.csv"); });

        // Outgoing edge to a queue-system that doesn't exist in this file.
        write_text_file(
            test_case_directory / "bad-out.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,,,[],[qX]\n");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "bad-out.csv"); });

        // Incoming edge from a queue-system that doesn't exist in this file.
        write_text_file(
            test_case_directory / "bad-in.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,,,[qX],[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "bad-in.csv"); });

        // Missing required column, invalid integers, etc. (same rules as Obj-B).
        write_text_file(test_case_directory / "missing-col.csv", "Queue sys ID\nq0\n");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "missing-col.csv"); });

        write_text_file(
            test_case_directory / "bad-rate.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,0,,,[],[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::load_multi_queue_config(test_case_directory / "bad-rate.csv"); });
    }

    // ------------------------------------------------------------------
    // Regression: a single-row Obj-E network must match Obj-B's own
    // "basic single queue" scenario exactly.
    // ------------------------------------------------------------------
    void test_single_queue_matches_obj_b_baseline(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "single-baseline");

        write_text_file(
            test_case_directory / "in0.wl",
            "Job Id,arrival time,Tasks\n"
            "j0,0,\"['0: W 10', '1: W 10']\"\n"
            "j1,10,\"['0: W 10']\"\n"
            "j2,20,\"['0: W 10', '1: W 10', '2: W 10']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,1,Sub-10*,10,in0.wl,out0.wl,[],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 61;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.time_steps_processed, 61LL);
        CHECK_EQ(result.total_errors, 0U);
        CHECK_EQ(result.per_queue_system.size(), 1U);
        CHECK_EQ(result.per_queue_system.at(0).jobs_written_to_sink, 3U);
        CHECK_EQ(result.per_queue_system.at(0).jobs_left_waiting, 0U);
        CHECK_EQ(result.per_queue_system.at(0).jobs_left_in_service, 0U);
        CHECK_EQ(result.per_queue_system.at(0).source_jobs_not_yet_arrived, 0U);

        const vector<qsim::Job> completed_jobs =
            qsim::load_workload(test_case_directory / "out0.wl");
        CHECK_EQ(completed_jobs.size(), 3U);
        CHECK_EQ(completed_jobs.at(0).arrival_time_step, 20LL);
        CHECK_EQ(completed_jobs.at(1).arrival_time_step, 30LL);
        CHECK_EQ(completed_jobs.at(2).arrival_time_step, 60LL);

        const string event_log_text = read_text_file(test_case_directory / "job.log");
        CHECK(event_log_text.find("0 j0 IN q0 ARRIVE-VIA in0.wl") != string::npos);
        CHECK(event_log_text.find("0 j0 IN q0 ENTERS-SERVER 0") != string::npos);
        CHECK(event_log_text.find("20 j0 IN q0 DEPART-VIA out0.wl") != string::npos);
        CHECK(event_log_text.find("20 j1 IN q0 ENTERS-SERVER 0") != string::npos);

        const string metrics_text = read_text_file(test_case_directory / "qmet.csv");
        CHECK_EQ(count_newline_characters(metrics_text), 62U); // header + t=0..60
        CHECK(metrics_text.find("0,q0,0,1,0\n") != string::npos);
    }

    // ------------------------------------------------------------------
    // A job that finishes at q0 and is still "Done" when handed to q1
    // departs straight through to q1's sink in the same tick.
    // ------------------------------------------------------------------
    void test_routes_finished_job_across_edge_to_sink(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "route-finished");

        write_text_file(
            test_case_directory / "in0.wl",
            "Job Id,arrival time,Tasks\n"
            "j0,0,\"['0: W 10']\"\n"
            "j1,5,\"['0: W 10']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,1,Sub-10,10,in0.wl,,[],[q1]\n"
            "q1,8,1,Sub-10,10,,out1.wl,[q0],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 40;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.total_errors, 0U);
        CHECK_EQ(result.per_queue_system.size(), 2U);
        CHECK_EQ(result.per_queue_system.at(0).jobs_departed_via_outgoing_edge, 2U);
        CHECK_EQ(result.per_queue_system.at(1).jobs_written_to_sink, 2U);

        const vector<qsim::Job> sink_jobs = qsim::load_workload(test_case_directory / "out1.wl");
        CHECK_EQ(sink_jobs.at(0).arrival_time_step, 10LL);
        CHECK_EQ(sink_jobs.at(1).arrival_time_step, 20LL);

        const string event_log_text = read_text_file(test_case_directory / "job.log");
        CHECK(event_log_text.find("10 j0 IN q0 DEPART-VIA q1") != string::npos);
        // The job reaches q1's sink in the SAME tick it departs q0 - no extra
        // tick of latency is introduced by crossing an edge.
        CHECK(event_log_text.find("10 j0 IN q1 DEPART-VIA out1.wl") != string::npos);
    }

    // ------------------------------------------------------------------
    // A job with leftover tasks continues service on the next queue-system,
    // instead of being (incorrectly) treated as finished.
    // ------------------------------------------------------------------
    void test_job_with_remaining_tasks_continues_service_after_edge(
        const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "route-continues");

        write_text_file(
            test_case_directory / "in0.wl",
            "Job Id,arrival time,Tasks\n"
            "j0,0,\"['0: W 10', '1: W 10']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,1,Sub-10,10,in0.wl,,[],[q1]\n"
            "q1,8,1,Sub-10,10,,out1.wl,[q0],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 30;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.total_errors, 0U);

        const vector<qsim::Job> sink_jobs = qsim::load_workload(test_case_directory / "out1.wl");
        CHECK_EQ(sink_jobs.size(), 1U);
        CHECK_EQ(sink_jobs.at(0).arrival_time_step, 20LL);
        CHECK(sink_jobs.at(0).tasks.empty());

        const string event_log_text = read_text_file(test_case_directory / "job.log");
        // Task "1" must start on q1, not be silently dropped.
        CHECK(event_log_text.find("10 j0 IN q1 TASK-START 1 \"W 10\"") != string::npos);
        CHECK(event_log_text.find("20 j0 IN q1 DEPART-VIA out1.wl") != string::npos);
    }

    // ------------------------------------------------------------------
    // Multiple outgoing edges to other queue-systems round-robin, exactly
    // like Obj-B's opaque-edge-label round robin, but delivering real jobs.
    // ------------------------------------------------------------------
    void test_round_robin_outgoing_edges_between_queue_systems(
        const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "round-robin");

        write_text_file(
            test_case_directory / "in0.wl",
            "Job Id,arrival time,Tasks\n"
            "a,0,\"['0: W 5']\"\n"
            "b,0,\"['0: W 5']\"\n"
            "c,5,\"['0: W 5']\"\n"
            "d,5,\"['0: W 5']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,2,Sub-5,5,in0.wl,,[],\"['q1', 'q2']\"\n"
            "q1,8,1,Sub-5,5,,out1.wl,[q0],[]\n"
            "q2,8,1,Sub-5,5,,out2.wl,[q0],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 20;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.total_errors, 0U);

        const vector<qsim::Job> sink1 = qsim::load_workload(test_case_directory / "out1.wl");
        const vector<qsim::Job> sink2 = qsim::load_workload(test_case_directory / "out2.wl");
        // Four jobs total, alternating between q1 and q2.
        CHECK_EQ(sink1.size() + sink2.size(), 4U);
        CHECK_EQ(sink1.size(), 2U);
        CHECK_EQ(sink2.size(), 2U);
    }

    // ------------------------------------------------------------------
    // Several queue-systems may share ONE sink file (as generated network
    // configs do). Every completed job must end up in that file, ordered by
    // the time it left the network - not just the last queue's jobs.
    // ------------------------------------------------------------------
    void test_queue_systems_sharing_one_sink_file_are_merged(
        const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "shared-sink");

        write_text_file(
            test_case_directory / "in0.wl",
            "Job Id,arrival time,Tasks\n"
            "a,0,\"['0: W 5']\"\n"
            "b,0,\"['0: W 5']\"\n"
            "c,5,\"['0: W 5']\"\n"
            "d,5,\"['0: W 5']\"\n");

        // Same network as the round-robin test, but q1 and q2 both name
        // "shared.wl" as their sink.
        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,2,Sub-5,5,in0.wl,,[],\"['q1', 'q2']\"\n"
            "q1,8,1,Sub-5,5,,shared.wl,[q0],[]\n"
            "q2,8,1,Sub-5,5,,shared.wl,[q0],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 20;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.total_errors, 0U);
        // Per-queue counters are unchanged by the merge.
        CHECK_EQ(result.per_queue_system.at(1).jobs_written_to_sink, 2U);
        CHECK_EQ(result.per_queue_system.at(2).jobs_written_to_sink, 2U);

        const vector<qsim::Job> merged_jobs =
            qsim::load_workload(test_case_directory / "shared.wl");
        CHECK_EQ(merged_jobs.size(), 4U);
        // Ordered by departure time; ties keep queue-system (row) order.
        CHECK_EQ(merged_jobs.at(0).job_id, "a");
        CHECK_EQ(merged_jobs.at(0).arrival_time_step, 5LL);
        CHECK_EQ(merged_jobs.at(1).job_id, "b");
        CHECK_EQ(merged_jobs.at(1).arrival_time_step, 5LL);
        CHECK_EQ(merged_jobs.at(2).job_id, "c");
        CHECK_EQ(merged_jobs.at(2).arrival_time_step, 10LL);
        CHECK_EQ(merged_jobs.at(3).job_id, "d");
        CHECK_EQ(merged_jobs.at(3).arrival_time_step, 10LL);
    }

    // ------------------------------------------------------------------
    // A downstream queue-system with no room reports QUEUE-FULL even
    // though the job arrived via an edge, not a source file.
    // ------------------------------------------------------------------
    void test_queue_full_error_when_delivered_across_edge(
        const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "queue-full-edge");

        // Three jobs land on q0 at once; q0 has 3 servers so all three finish
        // their q0 work together. Each still has 1 unit of work left ("W 2"
        // minus Sub-1), so they are NOT "Done" yet - they all try to cross
        // into q1, which has only one server and zero waiting room.
        write_text_file(
            test_case_directory / "in0.wl",
            "Job Id,arrival time,Tasks\n"
            "a,0,\"['0: W 2']\"\n"
            "b,0,\"['0: W 2']\"\n"
            "c,0,\"['0: W 2']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,3,Sub-1,1,in0.wl,,[],[q1]\n"
            "q1,0,1,Sub-5*,5,,out1.wl,[q0],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 10;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        // Exactly one of the three jobs gets a server on q1; the other two
        // have nowhere to wait (max queue size 0) and are dropped as errors.
        CHECK_EQ(result.total_errors, 2U);
        CHECK_EQ(result.per_queue_system.at(1).errors, 2U);

        const string event_log_text = read_text_file(test_case_directory / "job.log");
        CHECK(count_occurrences(event_log_text, "IN q1 ERROR QUEUE-FULL") == 2U);
    }

    // ------------------------------------------------------------------
    // A queue-system with leftover tasks, no sink, and no outgoing edge
    // reports NO-OUT-EDGE, same as Obj-B.
    // ------------------------------------------------------------------
    void test_no_out_edge_error(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "no-out-edge");

        write_text_file(test_case_directory / "in0.wl",
                        "Job Id,arrival time,Tasks\n"
                        "j,0,\"['0: W 10']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-5,1,in0.wl,,[],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.total_errors, 1U);
        CHECK(read_text_file(test_case_directory / "job.log").find("IN q0 ERROR NO-OUT-EDGE") != string::npos);
    }

    // ------------------------------------------------------------------
    // Each queue-system tracks its own source file independently.
    // ------------------------------------------------------------------
    void test_independent_sources_per_queue_system(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "independent-sources");

        write_text_file(test_case_directory / "inA.wl",
                        "Job Id,arrival time,Tasks\n"
                        "a0,0,[]\n");
        write_text_file(test_case_directory / "inB.wl",
                        "Job Id,arrival time,Tasks\n"
                        "b0,0,[]\n"
                        "b1,50,[]\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "qa,1,1,Sub-1,1,inA.wl,outA.wl,[],[]\n"
            "qb,1,1,Sub-1,1,inB.wl,outB.wl,[],[]\n");

        mqsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 10; // qb's second job (t=50) never arrives
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(result.per_queue_system.at(0).queue_system_id, "qa");
        CHECK_EQ(result.per_queue_system.at(0).jobs_written_to_sink, 1U);
        CHECK_EQ(result.per_queue_system.at(0).source_jobs_not_yet_arrived, 0U);

        CHECK_EQ(result.per_queue_system.at(1).queue_system_id, "qb");
        CHECK_EQ(result.per_queue_system.at(1).jobs_written_to_sink, 1U);
        CHECK_EQ(result.per_queue_system.at(1).source_jobs_not_yet_arrived, 1U); // b1 never arrived
    }

    // ------------------------------------------------------------------
    // A zero time limit produces only the metrics header, for every
    // queue-system in the network.
    // ------------------------------------------------------------------
    void test_zero_time_limit_and_invalid_options(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "zero-time-limit");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,,,[],[]\n"
            "q1,1,1,Sub-1,1,,,[],[]\n");

        mqsim::SimulationOptions zero_time_limit_options;
        zero_time_limit_options.time_limit = 0;
        zero_time_limit_options.metrics_output_path = test_case_directory / "m.csv";
        zero_time_limit_options.events_output_path = test_case_directory / "e.log";

        const mqsim::SimulationResult result =
            mqsim::run_multi_simulation(test_case_directory / "config.csv", zero_time_limit_options);

        CHECK_EQ(result.time_steps_processed, 0LL);
        CHECK_EQ(count_newline_characters(read_text_file(test_case_directory / "m.csv")), 1U);

        // A negative time limit is invalid.
        mqsim::SimulationOptions negative_time_limit_options;
        negative_time_limit_options.time_limit = -1;
        negative_time_limit_options.metrics_output_path = test_case_directory / "n.csv";
        negative_time_limit_options.events_output_path = test_case_directory / "n.log";
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::run_multi_simulation(test_case_directory / "config.csv",
                                                                           negative_time_limit_options); });

        // A directory cannot be used as an output file.
        mqsim::SimulationOptions invalid_events_path_options;
        invalid_events_path_options.metrics_output_path = test_case_directory / "ok.csv";
        file_system::create_directory(test_case_directory / "events-dir");
        invalid_events_path_options.events_output_path = test_case_directory / "events-dir";
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::run_multi_simulation(test_case_directory / "config.csv",
                                                                           invalid_events_path_options); });
    }

    // ------------------------------------------------------------------
    // Error paths and uncommon-but-valid configurations that the other
    // tests don't happen to exercise: blank/non-numeric integer fields,
    // negative sizes, zero servers, single-quoted source/sink paths,
    // absolute source/sink paths, a directory used as the metrics path,
    // a Queue sys ID that needs CSV escaping in the metrics output, and a
    // multi-step server-function chain (e.g. "Sub-5,Sub-10") actually
    // running to completion (not just parsed).
    // ------------------------------------------------------------------
    void test_additional_error_paths_and_uncommon_configs(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "extra");

        auto check_invalid_row = [&](const string &row, const string &fixture_name)
        {
            write_text_file(
                test_case_directory / fixture_name,
                "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n" +
                    row + "\n");
            check_that_exception_is_thrown([&]
                                           { (void)mqsim::load_multi_queue_config(test_case_directory / fixture_name); });
        };

        // Blank integer field ("missing integer value for ...").
        check_invalid_row("q0,,1,Sub-1,1,,,[],[]", "blank-max.csv");
        // Non-numeric integer field: stoll can't parse any of it at all
        // ("invalid integer for ...", the immediate-throw branch).
        check_invalid_row("q0,abc,1,Sub-1,1,,,[],[]", "bad-int.csv");
        // Integer field with trailing garbage: stoll parses a prefix but
        // doesn't consume the whole field ("invalid integer for ...", the
        // partial-parse branch - a different code path from the one above).
        check_invalid_row("q0,1x,1,Sub-1,1,,,[],[]", "bad-int-partial.csv");
        // Negative Max Queue Size.
        check_invalid_row("q0,-1,1,Sub-1,1,,,[],[]", "negative-max.csv");
        // Zero servers.
        check_invalid_row("q0,1,0,Sub-1,1,,,[],[]", "zero-servers.csv");
        // Empty Queue sys ID.
        check_invalid_row(",1,1,Sub-1,1,,,[],[]", "empty-id.csv");

        // Single-quoted source/sink paths must be unwrapped, just like Obj-B.
        write_text_file(test_case_directory / "in.wl", "Job Id,arrival time,Tasks\nj,0,[]\n");
        write_text_file(
            test_case_directory / "quoted-path.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,'in.wl','out.wl',[],[]\n");
        const mqsim::MultiQueueConfig quoted_path_config =
            mqsim::load_multi_queue_config(test_case_directory / "quoted-path.csv");
        CHECK_EQ(quoted_path_config.queue_systems.at(0).source_workload_path, "in.wl");
        CHECK_EQ(quoted_path_config.queue_systems.at(0).sink_workload_path, "out.wl");

        // Absolute source/sink paths pass through resolve_path_relative_to_configuration
        // unchanged (the "already absolute" branch).
        const file_system::path absolute_source_path = test_case_directory / "abs-in.wl";
        write_text_file(absolute_source_path, "Job Id,arrival time,Tasks\nabs,0,[]\n");
        const file_system::path absolute_sink_path = test_case_directory / "abs-out.wl";

        write_text_file(
            test_case_directory / "absolute-config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1," +
                absolute_source_path.string() + "," + absolute_sink_path.string() + ",[],[]\n");

        mqsim::SimulationOptions absolute_path_options;
        absolute_path_options.metrics_output_path = test_case_directory / "abs-m.csv";
        absolute_path_options.events_output_path = test_case_directory / "abs-e.log";
        const mqsim::SimulationResult absolute_path_result =
            mqsim::run_multi_simulation(test_case_directory / "absolute-config.csv", absolute_path_options);
        CHECK_EQ(absolute_path_result.per_queue_system.at(0).jobs_written_to_sink, 1U);
        CHECK(file_system::exists(absolute_sink_path));

        // A directory cannot be used as the metrics output path.
        write_text_file(
            test_case_directory / "simple-config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,,,[],[]\n");

        mqsim::SimulationOptions invalid_metrics_path_options;
        file_system::create_directory(test_case_directory / "metrics-dir");
        invalid_metrics_path_options.metrics_output_path = test_case_directory / "metrics-dir";
        invalid_metrics_path_options.events_output_path = test_case_directory / "ok.log";
        check_that_exception_is_thrown([&]
                                       { (void)mqsim::run_multi_simulation(test_case_directory / "simple-config.csv",
                                                                           invalid_metrics_path_options); });

        // A Queue sys ID containing a double quote forces CSV escaping when
        // the metrics row is written back out.
        write_text_file(
            test_case_directory / "quoted-id.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "\"a\"\"b\",1,1,Sub-1,1,,,[],[]\n");

        mqsim::SimulationOptions quoted_id_options;
        quoted_id_options.time_limit = 1;
        quoted_id_options.metrics_output_path = test_case_directory / "quoted-id-m.csv";
        quoted_id_options.events_output_path = test_case_directory / "quoted-id-e.log";
        (void)mqsim::run_multi_simulation(test_case_directory / "quoted-id.csv", quoted_id_options);

        const string quoted_id_metrics_text = read_text_file(test_case_directory / "quoted-id-m.csv");
        CHECK(quoted_id_metrics_text.find("\"a\"\"b\"") != string::npos);

        // A multi-step server-function chain ("Sub-5,Sub-10") must actually
        // run both steps on the same server, back to back.
        // "W 15" is chosen so Sub-5 (15 -> 10) then Sub-10 (10 -> removed,
        // since 10 <= 10) finishes the task exactly at the end of the
        // chain, so the job departs to the sink rather than needing a
        // repeat ('*') or an outgoing edge.
        write_text_file(
            test_case_directory / "chain-in.wl",
            "Job Id,arrival time,Tasks\n"
            "j,0,\"['t: W 15']\"\n");
        write_text_file(
            test_case_directory / "chain-config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q,1,1,\"Sub-5,Sub-10\",1,chain-in.wl,chain-out.wl,[],[]\n");

        mqsim::SimulationOptions chain_options;
        chain_options.metrics_output_path = test_case_directory / "chain-m.csv";
        chain_options.events_output_path = test_case_directory / "chain-e.log";
        const mqsim::SimulationResult chain_result =
            mqsim::run_multi_simulation(test_case_directory / "chain-config.csv", chain_options);

        CHECK_EQ(chain_result.total_errors, 0U);
        CHECK_EQ(chain_result.per_queue_system.at(0).jobs_written_to_sink, 1U);

        const string chain_event_log = read_text_file(test_case_directory / "chain-e.log");
        // After Sub-5 (W15 -> W10), the chain continues on the SAME server
        // with Sub-10 (W10 -> task removed) without an EXITS-SERVER in
        // between the two steps.
        CHECK(chain_event_log.find("1 j IN q TASK-START t \"W 10\"") != string::npos);
        CHECK(chain_event_log.find("1 j IN q EXITS-SERVER") == string::npos);
        CHECK(chain_event_log.find("2 j IN q EXITS-SERVER 0") != string::npos);
        CHECK(chain_event_log.find("2 j IN q DEPART-VIA chain-out.wl") != string::npos);
    }

} // namespace

int main()
{
    const file_system::path test_root_directory =
        file_system::temp_directory_path() / "multi-queue-sim-tests";

    file_system::remove_all(test_root_directory);
    file_system::create_directories(test_root_directory);

    test_load_multi_queue_config(test_root_directory);
    test_single_queue_matches_obj_b_baseline(test_root_directory);
    test_routes_finished_job_across_edge_to_sink(test_root_directory);
    test_job_with_remaining_tasks_continues_service_after_edge(test_root_directory);
    test_round_robin_outgoing_edges_between_queue_systems(test_root_directory);
    test_queue_systems_sharing_one_sink_file_are_merged(test_root_directory);
    test_queue_full_error_when_delivered_across_edge(test_root_directory);
    test_no_out_edge_error(test_root_directory);
    test_independent_sources_per_queue_system(test_root_directory);
    test_zero_time_limit_and_invalid_options(test_root_directory);
    test_additional_error_paths_and_uncommon_configs(test_root_directory);

    file_system::remove_all(test_root_directory);

    if (failed_check_count != 0)
    {
        cerr << failed_check_count << " of " << total_check_count << " checks failed\n";
        return 1;
    }

    cout << "PASS: " << total_check_count << " checks\n";
    return 0;
}