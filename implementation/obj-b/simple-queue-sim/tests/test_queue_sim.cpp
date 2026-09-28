#include "queue_sim.hpp"

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

    // Small test helpers used throughout this file.
    int failed_check_count = 0;
    int total_check_count = 0;

// Record a failed check and continue running the remaining tests.
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

// Compare two values and report the source line when they differ.
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

    // Check that invalid input causes an exception.
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

    // Write a text file used by a test case.
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

    // Read a whole text file into one string.
    string read_text_file(const file_system::path &input_path)
    {
        ifstream input_file(input_path);
        ostringstream complete_contents;
        complete_contents << input_file.rdbuf();
        return complete_contents.str();
    }

    // Count rows by counting newline characters.
    size_t count_newline_characters(const string &text)
    {
        size_t newline_count = 0;

        for (char current_character : text)
        {
            if (current_character == '\n')
            {
                ++newline_count;
            }
        }

        return newline_count;
    }

    // Create a clean folder for one test scenario.
    file_system::path create_clean_test_case_directory(
        const file_system::path &test_root_directory,
        const string &test_case_name)
    {

        const file_system::path test_case_directory = test_root_directory / test_case_name;
        file_system::remove_all(test_case_directory);
        file_system::create_directories(test_case_directory);
        return test_case_directory;
    }

    // Parser tests
    void test_parsing_helpers()
    {
        // Basic CSV parsing, including quoted fields and escaped quotes.
        CHECK_EQ(qsim::parse_csv_line("a,b,c").size(), 3U);

        const vector<string> quoted_csv_fields =
            qsim::parse_csv_line("a,\"b,c\",\"d\"\"e\"");
        CHECK_EQ(quoted_csv_fields.at(1), "b,c");
        CHECK_EQ(quoted_csv_fields.at(2), "d\"e");

        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_csv_line("\"oops"); });

        // Parse task lists in the "<tid>: W N" format.
        const vector<qsim::Task> parsed_tasks =
            qsim::parse_tasks("['0: W 10', \"1: W 20\"]");
        CHECK_EQ(parsed_tasks.size(), 2U);
        CHECK_EQ(parsed_tasks.at(0).task_id, "0");
        CHECK_EQ(parsed_tasks.at(1).remaining_work, 20LL);
        CHECK_EQ(qsim::serialize_tasks(parsed_tasks), "['0: W 10', '1: W 20']");
        CHECK_EQ(qsim::serialize_tasks({}), "[]");
        CHECK(qsim::parse_tasks("[]").empty());

        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_tasks("['bad task']"); });
        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_tasks("['0: W 0']"); });
        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_tasks("['0: W 1]"); });

        // Test one Sub-X, function chains, and '*'.
        const qsim::ServerFunctionChain repeating_server_function =
            qsim::parse_server_function("Sub-10*");
        CHECK(repeating_server_function.repeat_until_tasks_complete);
        CHECK_EQ(repeating_server_function.subtraction_amounts.at(0), 10LL);

        const qsim::ServerFunctionChain chained_server_function =
            qsim::parse_server_function("Sub-5, Sub-10");
        CHECK(!chained_server_function.repeat_until_tasks_complete);
        CHECK_EQ(chained_server_function.subtraction_amounts.size(), 2U);

        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_server_function(""); });
        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_server_function("*"); });
        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_server_function("Add-10"); });
        check_that_exception_is_thrown([]
                                       { (void)qsim::parse_server_function("Sub-0"); });

        // Test empty edge lists, quoted names, commas, and semicolons.
        CHECK(qsim::parse_edge_list("[]").empty());
        CHECK(qsim::parse_edge_list("[ ]").empty());

        const vector<string> parsed_edges =
            qsim::parse_edge_list("['q1', \"q2\"; q3]");
        CHECK_EQ(parsed_edges.size(), 3U);
        CHECK_EQ(parsed_edges.at(2), "q3");

        const vector<string> edge_with_escaped_quote =
            qsim::parse_edge_list("['a\\\'b']");
        CHECK_EQ(edge_with_escaped_quote.at(0), "a'b");
    }

    // Configuration/workload file tests
    void test_configuration_and_workload_file_io(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "io");

        write_text_file(
            test_case_directory / "cfg.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,8,2,\"Sub-5,Sub-10\",3,in.wl,out.wl,\"['up']\",\"['down']\"\n");

        const qsim::QueueConfig loaded_configuration =
            qsim::load_queue_config(test_case_directory / "cfg.csv");

        CHECK_EQ(loaded_configuration.queue_system_id, "q0");
        CHECK_EQ(loaded_configuration.maximum_queue_size, 8U);
        CHECK_EQ(loaded_configuration.number_of_servers, 2U);
        CHECK_EQ(loaded_configuration.server_function_rate, 3LL);
        CHECK_EQ(loaded_configuration.incoming_edges.at(0), "up");
        CHECK_EQ(loaded_configuration.outgoing_edges.at(0), "down");

        // Write jobs out of order and make sure they are sorted by arrival time.
        write_text_file(
            test_case_directory / "jobs.wl",
            "Job Id,arrival time,Tasks\n"
            "j1,3,\"['0: W 2']\"\n"
            "\n"
            "j0,1,\"['0: W 1', '1: W 4']\"\n"
            "j2,3,[]\n");

        const vector<qsim::Job> loaded_jobs =
            qsim::load_workload(test_case_directory / "jobs.wl");
        CHECK_EQ(loaded_jobs.size(), 3U);
        CHECK_EQ(loaded_jobs.at(0).job_id, "j0");
        CHECK_EQ(loaded_jobs.at(1).job_id, "j1");
        CHECK_EQ(loaded_jobs.at(2).job_id, "j2");

        // Writing and reading a workload should preserve its data.
        qsim::write_workload(test_case_directory / "nested" / "written.wl", loaded_jobs);
        const vector<qsim::Job> round_trip_jobs =
            qsim::load_workload(test_case_directory / "nested" / "written.wl");
        CHECK_EQ(round_trip_jobs, loaded_jobs);

        vector<qsim::Job> jobs_with_quoted_identifier = loaded_jobs;
        jobs_with_quoted_identifier.front().job_id = "a\"b";
        qsim::write_workload(test_case_directory / "quoted.wl", jobs_with_quoted_identifier);
        CHECK_EQ(qsim::load_workload(test_case_directory / "quoted.wl"),
                 jobs_with_quoted_identifier);

        // Source and sink paths may be wrapped in single quotes.
        write_text_file(
            test_case_directory / "quoted-path.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,'in.wl','out.wl',[],[]\n");

        const qsim::QueueConfig configuration_with_quoted_paths =
            qsim::load_queue_config(test_case_directory / "quoted-path.csv");
        CHECK_EQ(configuration_with_quoted_paths.source_workload_path, "in.wl");
        CHECK_EQ(configuration_with_quoted_paths.sink_workload_path, "out.wl");

        // Invalid configuration files.
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_queue_config(test_case_directory / "missing.csv"); });

        write_text_file(test_case_directory / "empty.csv", "");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_queue_config(test_case_directory / "empty.csv"); });

        write_text_file(test_case_directory / "missing-col.csv", "Queue sys ID\nq0\n");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_queue_config(test_case_directory / "missing-col.csv"); });

        write_text_file(
            test_case_directory / "multi.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q0,1,1,Sub-1,1,,,[],[]\n"
            "q1,1,1,Sub-1,1,,,[],[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_queue_config(test_case_directory / "multi.csv"); });

        // Helper for several invalid one-row configurations.
        auto check_invalid_configuration_row = [&](const string &invalid_row,
                                                   const string &fixture_filename)
        {
            write_text_file(
                test_case_directory / fixture_filename,
                "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n" +
                    invalid_row + "\n");

            check_that_exception_is_thrown([&]
                                           { (void)qsim::load_queue_config(test_case_directory / fixture_filename); });
        };

        check_invalid_configuration_row(",1,1,Sub-1,1,,,[],[]", "bad-id.csv");
        check_invalid_configuration_row("q0,-1,1,Sub-1,1,,,[],[]", "bad-max.csv");
        check_invalid_configuration_row("q0,,1,Sub-1,1,,,[],[]", "missing-max.csv");
        check_invalid_configuration_row("q0,x,1,Sub-1,1,,,[],[]", "bad-int.csv");
        check_invalid_configuration_row("q0,1,0,Sub-1,1,,,[],[]", "bad-servers.csv");
        check_invalid_configuration_row("q0,1,1,Sub-1,0,,,[],[]", "bad-rate.csv");
        check_invalid_configuration_row("q0,1,1,Add-1,1,,,[],[]", "bad-func.csv");

        // Invalid workload files.
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_workload(test_case_directory / "none.wl"); });

        write_text_file(test_case_directory / "empty.wl", "");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_workload(test_case_directory / "empty.wl"); });

        write_text_file(test_case_directory / "bad-head.wl",
                        "Job Id,arrival time\nj0,0\n");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_workload(test_case_directory / "bad-head.wl"); });

        write_text_file(test_case_directory / "bad-id.wl",
                        "Job Id,arrival time,Tasks\n,0,[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_workload(test_case_directory / "bad-id.wl"); });

        write_text_file(test_case_directory / "bad-arrival.wl",
                        "Job Id,arrival time,Tasks\nj0,-1,[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_workload(test_case_directory / "bad-arrival.wl"); });

        write_text_file(test_case_directory / "bad-arrival2.wl",
                        "Job Id,arrival time,Tasks\nj0,1x,[]\n");
        check_that_exception_is_thrown([&]
                                       { (void)qsim::load_workload(test_case_directory / "bad-arrival2.wl"); });

        // A directory cannot be used as an output file.
        file_system::create_directory(test_case_directory / "directory-as-file");
        check_that_exception_is_thrown([&]
                                       { qsim::write_workload(test_case_directory / "directory-as-file", loaded_jobs); });
    }

    // Full simulation scenarios
    void test_basic_single_queue_simulation(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "basic");

        // Basic example: one server with Sub-10* and three jobs.
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

        qsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 61;
        simulation_options.metrics_output_path = test_case_directory / "qmet.csv";
        simulation_options.events_output_path = test_case_directory / "job.log";

        const qsim::SimulationResult simulation_result =
            qsim::run_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(simulation_result.time_steps_processed, 61LL);
        CHECK_EQ(simulation_result.total_errors, 0U);
        CHECK_EQ(simulation_result.jobs_written_to_sink, 3U);
        CHECK_EQ(simulation_result.jobs_departed_via_outgoing_edge, 0U);
        CHECK_EQ(simulation_result.jobs_left_waiting, 0U);
        CHECK_EQ(simulation_result.jobs_left_in_service, 0U);
        CHECK_EQ(simulation_result.source_jobs_not_yet_arrived, 0U);

        const vector<qsim::Job> completed_jobs =
            qsim::load_workload(test_case_directory / "out0.wl");
        CHECK_EQ(completed_jobs.size(), 3U);
        CHECK_EQ(completed_jobs.at(0).arrival_time_step, 20LL);
        CHECK_EQ(completed_jobs.at(1).arrival_time_step, 30LL);
        CHECK_EQ(completed_jobs.at(2).arrival_time_step, 60LL);
        CHECK(completed_jobs.at(0).tasks.empty());

        const string event_log_text = read_text_file(test_case_directory / "job.log");
        CHECK(event_log_text.find("0 j0 IN q0 ARRIVE-VIA in0.wl") != string::npos);
        CHECK(event_log_text.find("0 j0 IN q0 ENTERS-SERVER 0") != string::npos);
        CHECK(event_log_text.find("10 j0 IN q0 TASK-STOP 0 \"W 10\"") != string::npos);
        CHECK(event_log_text.find("20 j0 IN q0 EXITS-SERVER 0") != string::npos);
        CHECK(event_log_text.find("20 j0 IN q0 DEPART-VIA out0.wl") != string::npos);
        CHECK(event_log_text.find("20 j1 IN q0 ENTERS-SERVER 0") != string::npos);
        CHECK(event_log_text.find("20 j2 IN q0 ARRIVE-VIA in0.wl") != string::npos);

        const string metrics_file_text = read_text_file(test_case_directory / "qmet.csv");
        CHECK_EQ(count_newline_characters(metrics_file_text), 62U); // header + t=0..60
        CHECK(metrics_file_text.find("0,q0,0,1,0\n") != string::npos);
    }

    void test_queue_full_error_and_time_limit(const file_system::path &test_root_directory)
    {
        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "queue-full");

        // One server and one waiting slot cause the third job to hit QUEUE-FULL.
        // The later job also checks that a time limit can stop before all arrivals.
        write_text_file(
            test_case_directory / "in.wl",
            "Job Id,arrival time,Tasks\n"
            "a,0,\"['0: W 1']\"\n"
            "b,0,\"['0: W 1']\"\n"
            "c,0,\"['0: W 1']\"\n"
            "future,20,\"['0: W 1']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q,1,1,Sub-1*,5,in.wl,out.wl,[],[]\n");

        qsim::SimulationOptions simulation_options;
        simulation_options.time_limit = 6;
        simulation_options.metrics_output_path = test_case_directory / "m.csv";
        simulation_options.events_output_path = test_case_directory / "e.log";

        const qsim::SimulationResult simulation_result =
            qsim::run_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(simulation_result.total_errors, 1U);
        CHECK_EQ(simulation_result.jobs_written_to_sink, 1U);
        CHECK_EQ(simulation_result.jobs_left_in_service, 1U);
        CHECK_EQ(simulation_result.source_jobs_not_yet_arrived, 1U);
        CHECK(read_text_file(test_case_directory / "e.log")
                  .find("0 c IN q ERROR QUEUE-FULL") != string::npos);
        CHECK(read_text_file(test_case_directory / "m.csv")
                  .find("0,q,1,1,1") != string::npos);
    }

    void test_no_outgoing_edge_error_and_round_robin_edges(
        const file_system::path &test_root_directory)
    {

        // An incomplete job with no sink or outgoing edge gets NO-OUT-EDGE.
        {
            const file_system::path test_case_directory =
                create_clean_test_case_directory(test_root_directory, "no-edge");

            write_text_file(test_case_directory / "in.wl",
                            "Job Id,arrival time,Tasks\n"
                            "j,0,\"['0: W 10']\"\n");

            write_text_file(
                test_case_directory / "config.csv",
                "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
                "q,1,1,Sub-5,1,in.wl,,[],[]\n");

            qsim::SimulationOptions simulation_options;
            simulation_options.metrics_output_path = test_case_directory / "m.csv";
            simulation_options.events_output_path = test_case_directory / "e.log";

            const qsim::SimulationResult simulation_result =
                qsim::run_simulation(test_case_directory / "config.csv", simulation_options);

            CHECK_EQ(simulation_result.time_steps_processed, 2LL);
            CHECK_EQ(simulation_result.total_errors, 1U);
            CHECK(read_text_file(test_case_directory / "e.log")
                      .find("1 j IN q ERROR NO-OUT-EDGE") != string::npos);
        }

        // With two outgoing edges, jobs alternate between the edges.
        {
            const file_system::path test_case_directory =
                create_clean_test_case_directory(test_root_directory, "edges");

            write_text_file(
                test_case_directory / "in.wl",
                "Job Id,arrival time,Tasks\n"
                "a,0,\"['0: W 10']\"\n"
                "b,0,\"['0: W 10']\"\n");

            write_text_file(
                test_case_directory / "config.csv",
                "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
                "q,0,2,Sub-5,1,in.wl,,[],\"['e0','e1']\"\n");

            qsim::SimulationOptions simulation_options;
            simulation_options.metrics_output_path = test_case_directory / "m.csv";
            simulation_options.events_output_path = test_case_directory / "e.log";

            const qsim::SimulationResult simulation_result =
                qsim::run_simulation(test_case_directory / "config.csv", simulation_options);

            CHECK_EQ(simulation_result.total_errors, 0U);
            CHECK_EQ(simulation_result.jobs_departed_via_outgoing_edge, 2U);

            const string event_log_text = read_text_file(test_case_directory / "e.log");
            CHECK(event_log_text.find("1 a IN q DEPART-VIA e0") != string::npos);
            CHECK(event_log_text.find("1 b IN q DEPART-VIA e1") != string::npos);
        }
    }

    void test_chained_server_function_partial_job_and_empty_job(
        const file_system::path &test_root_directory)
    {

        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "chain");

        write_text_file(
            test_case_directory / "in.wl",
            "Job Id,arrival time,Tasks\n"
            "empty,0,[]\n"
            "partial,1,\"['t: W 20']\"\n");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q,2,1,\"Sub-5,Sub-10\",2,in.wl,out.wl,[],\"['next']\"\n");

        qsim::SimulationOptions simulation_options;
        simulation_options.metrics_output_path = test_case_directory / "m.csv";
        simulation_options.events_output_path = test_case_directory / "e.log";

        const qsim::SimulationResult simulation_result =
            qsim::run_simulation(test_case_directory / "config.csv", simulation_options);

        CHECK_EQ(simulation_result.jobs_written_to_sink, 1U);            // empty job -> sink immediately
        CHECK_EQ(simulation_result.jobs_departed_via_outgoing_edge, 1U); // partial ends with W 5
        CHECK_EQ(simulation_result.total_errors, 0U);

        const vector<qsim::Job> sink_jobs =
            qsim::load_workload(test_case_directory / "out.wl");
        CHECK_EQ(sink_jobs.at(0).job_id, "empty");
        CHECK_EQ(sink_jobs.at(0).arrival_time_step, 0LL);

        const string event_log_text = read_text_file(test_case_directory / "e.log");
        CHECK(event_log_text.find("3 partial IN q TASK-START t \"W 15\"") != string::npos);
        CHECK(event_log_text.find("5 partial IN q DEPART-VIA next") != string::npos);
    }

    void test_zero_time_limit_no_source_and_output_path_errors(
        const file_system::path &test_root_directory)
    {

        const file_system::path test_case_directory =
            create_clean_test_case_directory(test_root_directory, "zero");

        write_text_file(
            test_case_directory / "config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q,0,1,Sub-1,1,,,[],[]\n");

        // A zero time limit produces only the metrics header.
        qsim::SimulationOptions zero_time_limit_options;
        zero_time_limit_options.time_limit = 0;
        zero_time_limit_options.metrics_output_path = test_case_directory / "m.csv";
        zero_time_limit_options.events_output_path = test_case_directory / "e.log";

        const qsim::SimulationResult zero_time_limit_result =
            qsim::run_simulation(test_case_directory / "config.csv", zero_time_limit_options);

        CHECK_EQ(zero_time_limit_result.time_steps_processed, 0LL);
        CHECK_EQ(count_newline_characters(read_text_file(test_case_directory / "m.csv")), 1U);

        // A negative time limit is invalid.
        qsim::SimulationOptions negative_time_limit_options;
        negative_time_limit_options.time_limit = -1;
        negative_time_limit_options.metrics_output_path = test_case_directory / "n.csv";
        negative_time_limit_options.events_output_path = test_case_directory / "n.log";
        check_that_exception_is_thrown([&]
                                       { (void)qsim::run_simulation(test_case_directory / "config.csv",
                                                                    negative_time_limit_options); });

        // A directory cannot be used as the event-log file.
        qsim::SimulationOptions invalid_event_path_options;
        invalid_event_path_options.metrics_output_path = test_case_directory / "ok.csv";
        file_system::create_directory(test_case_directory / "events-dir");
        invalid_event_path_options.events_output_path = test_case_directory / "events-dir";
        check_that_exception_is_thrown([&]
                                       { (void)qsim::run_simulation(test_case_directory / "config.csv",
                                                                    invalid_event_path_options); });

        // A directory cannot be used as the metrics file either.
        qsim::SimulationOptions invalid_metrics_path_options;
        file_system::create_directory(test_case_directory / "metrics-dir");
        invalid_metrics_path_options.metrics_output_path = test_case_directory / "metrics-dir";
        invalid_metrics_path_options.events_output_path = test_case_directory / "ok.log";
        check_that_exception_is_thrown([&]
                                       { (void)qsim::run_simulation(test_case_directory / "config.csv",
                                                                    invalid_metrics_path_options); });

        // Absolute source and sink paths stay unchanged.
        const file_system::path absolute_source_path = test_case_directory / "absolute.wl";
        write_text_file(absolute_source_path, "Job Id,arrival time,Tasks\nabs,0,[]\n");

        const file_system::path absolute_sink_path = test_case_directory / "absolute-out.wl";
        write_text_file(
            test_case_directory / "absolute-config.csv",
            "Queue sys ID,Max Queue Size,# of servers,Server Function,Server Function Rate,source,sink,Edges in,Edges out\n"
            "q,0,1,Sub-1,1," +
                absolute_source_path.string() + "," +
                absolute_sink_path.string() + ",[],[]\n");

        qsim::SimulationOptions absolute_path_options;
        absolute_path_options.metrics_output_path = test_case_directory / "abs-m.csv";
        absolute_path_options.events_output_path = test_case_directory / "abs-e.log";

        const qsim::SimulationResult absolute_path_result =
            qsim::run_simulation(test_case_directory / "absolute-config.csv",
                                 absolute_path_options);

        CHECK_EQ(absolute_path_result.jobs_written_to_sink, 1U);
        CHECK(file_system::exists(absolute_sink_path));
    }

} // namespace

int main()
{
    const file_system::path test_root_directory =
        file_system::temp_directory_path() / "simple-queue-sim-tests";

    // Remove old test files before starting.
    file_system::remove_all(test_root_directory);
    file_system::create_directories(test_root_directory);

    test_parsing_helpers();
    test_configuration_and_workload_file_io(test_root_directory);
    test_basic_single_queue_simulation(test_root_directory);
    test_queue_full_error_and_time_limit(test_root_directory);
    test_no_outgoing_edge_error_and_round_robin_edges(test_root_directory);
    test_chained_server_function_partial_job_and_empty_job(test_root_directory);
    test_zero_time_limit_no_source_and_output_path_errors(test_root_directory);

    file_system::remove_all(test_root_directory);

    if (failed_check_count != 0)
    {
        cerr << failed_check_count << " of " << total_check_count << " checks failed\n";
        return 1;
    }

    cout << "PASS: " << total_check_count << " checks\n";
    return 0;
}
