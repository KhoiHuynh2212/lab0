#include "multi_queue_sim.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

using namespace std;

namespace mqsim
{
    namespace
    {

        // ------------------------------------------------------------------
        // Small parsing helpers (kept local: Obj-B's equivalents live in an
        // anonymous namespace inside queue_sim.cpp and are not exported).
        // ------------------------------------------------------------------

        string trim_whitespace(const string &input_text)
        {
            const size_t first = input_text.find_first_not_of(" \t\r\n");
            if (first == string::npos)
            {
                return {};
            }
            const size_t last = input_text.find_last_not_of(" \t\r\n");
            return input_text.substr(first, last - first + 1);
        }

        string make_lowercase_alphanumeric(const string &input_text)
        {
            string normalized;
            for (unsigned char c : input_text)
            {
                if (isalnum(c))
                {
                    normalized.push_back(static_cast<char>(tolower(c)));
                }
            }
            return normalized;
        }

        long long parse_required_integer(const string &input_text, const string &field_name)
        {
            const string trimmed = trim_whitespace(input_text);
            if (trimmed.empty())
            {
                throw runtime_error("missing integer value for " + field_name);
            }

            size_t consumed = 0;
            long long value = 0;
            try
            {
                value = stoll(trimmed, &consumed);
            }
            catch (const exception &)
            {
                throw runtime_error("invalid integer for " + field_name + ": " + input_text);
            }

            if (consumed != trimmed.size())
            {
                throw runtime_error("invalid integer for " + field_name + ": " + input_text);
            }

            return value;
        }

        string remove_matching_outer_quotes(const string &input_text)
        {
            string unquoted = trim_whitespace(input_text);
            const bool single = unquoted.size() >= 2 && unquoted.front() == '\'' && unquoted.back() == '\'';
            const bool doubled = unquoted.size() >= 2 && unquoted.front() == '"' && unquoted.back() == '"';
            if (single || doubled)
            {
                unquoted = unquoted.substr(1, unquoted.size() - 2);
            }
            return trim_whitespace(unquoted);
        }

        string escape_csv_field(const string &field_value)
        {
            const bool requires_quotes = field_value.find_first_of(",\"\r\n") != string::npos;
            if (!requires_quotes)
            {
                return field_value;
            }
            string escaped = "\"";
            for (char c : field_value)
            {
                if (c == '"')
                {
                    escaped += "\"\"";
                }
                else
                {
                    escaped.push_back(c);
                }
            }
            escaped.push_back('"');
            return escaped;
        }

        map<string, size_t> build_normalized_header_index(const vector<string> &header_fields)
        {
            map<string, size_t> indexes;
            for (size_t i = 0; i < header_fields.size(); ++i)
            {
                indexes[make_lowercase_alphanumeric(header_fields[i])] = i;
            }
            return indexes;
        }

        size_t find_required_column_index(const map<string, size_t> &header_indexes,
                                          const vector<string> &accepted_names,
                                          const string &display_name)
        {
            for (const string &name : accepted_names)
            {
                const auto it = header_indexes.find(name);
                if (it != header_indexes.end())
                {
                    return it->second;
                }
            }
            throw runtime_error("missing required column: " + display_name);
        }

        string get_trimmed_field(const vector<string> &row_fields, size_t column_index)
        {
            return column_index < row_fields.size() ? trim_whitespace(row_fields[column_index]) : string{};
        }

        filesystem::path resolve_path_relative_to_configuration(
            const filesystem::path &configuration_path,
            const string &configured_path_text)
        {
            filesystem::path configured_path = configured_path_text;
            if (configured_path.empty() || configured_path.is_absolute())
            {
                return configured_path;
            }
            return configuration_path.parent_path() / configured_path;
        }

        void create_parent_directories_if_needed(const filesystem::path &output_path)
        {
            if (!output_path.parent_path().empty())
            {
                filesystem::create_directories(output_path.parent_path());
            }
        }

        string format_task_body(const qsim::Task &task)
        {
            return "W " + to_string(task.remaining_work);
        }

        // ------------------------------------------------------------------
        // Runtime state
        // ------------------------------------------------------------------

        struct ServerRuntimeState
        {
            bool is_busy = false;
            qsim::Job current_job;
            long long service_time_remaining = 0;
            size_t server_function_step_index = 0;
        };

        struct QueueSystemRuntime
        {
            const QueueSystemConfig *config = nullptr;

            deque<qsim::Job> waiting_jobs;
            vector<ServerRuntimeState> server_states;

            vector<qsim::Job> source_workload_jobs;
            size_t next_source_job_index = 0;

            vector<qsim::Job> completed_sink_jobs;

            size_t cumulative_error_count = 0;
            size_t outgoing_edge_departure_count = 0;
            size_t next_outgoing_edge_index = 0;

            bool any_server_is_busy() const
            {
                return any_of(server_states.begin(), server_states.end(),
                              [](const ServerRuntimeState &s)
                              { return s.is_busy; });
            }

            bool is_finished() const
            {
                return next_source_job_index >= source_workload_jobs.size() &&
                       waiting_jobs.empty() &&
                       !any_server_is_busy();
            }
        };

    } // namespace

    // ----------------------------------------------------------------------
    // Public: load_multi_queue_config
    // ----------------------------------------------------------------------

    MultiQueueConfig load_multi_queue_config(const filesystem::path &configuration_path)
    {
        ifstream configuration_file(configuration_path);
        if (!configuration_file)
        {
            throw runtime_error("cannot open multi-queue configuration: " + configuration_path.string());
        }

        string current_line;
        if (!getline(configuration_file, current_line))
        {
            throw runtime_error("multi-queue configuration is empty: " + configuration_path.string());
        }

        const vector<string> header_fields = qsim::parse_csv_line(current_line);
        const map<string, size_t> header_indexes = build_normalized_header_index(header_fields);

        const size_t queue_id_column =
            find_required_column_index(header_indexes, {"queuesysid", "queuesystemid", "qsid"}, "Queue sys ID");
        const size_t maximum_queue_size_column =
            find_required_column_index(header_indexes, {"maxqueuesize", "queuesize"}, "Max Queue Size");
        const size_t server_count_column =
            find_required_column_index(header_indexes, {"ofservers", "numservers", "numberofservers", "servers"}, "# of servers");
        const size_t server_function_column =
            find_required_column_index(header_indexes, {"serverfunction", "function"}, "Server Function");
        const size_t server_function_rate_column =
            find_required_column_index(header_indexes, {"serverfunctionrate", "functionrate", "rate"}, "Server Function Rate");
        const size_t source_column =
            find_required_column_index(header_indexes, {"source"}, "source");
        const size_t sink_column =
            find_required_column_index(header_indexes, {"sink"}, "sink");
        const size_t incoming_edges_column =
            find_required_column_index(header_indexes, {"edgesin", "incomingedges"}, "Edges in");
        const size_t outgoing_edges_column =
            find_required_column_index(header_indexes, {"edgesout", "outgoingedges"}, "Edges out");

        vector<string> nonempty_rows;
        while (getline(configuration_file, current_line))
        {
            if (!trim_whitespace(current_line).empty())
            {
                nonempty_rows.push_back(current_line);
            }
        }

        if (nonempty_rows.empty())
        {
            throw runtime_error("multi-queue configuration has no queue-system rows: " +
                                configuration_path.string());
        }

        MultiQueueConfig multi_queue_config;
        set<string> seen_ids;

        for (const string &row_text : nonempty_rows)
        {
            const vector<string> fields = qsim::parse_csv_line(row_text);

            QueueSystemConfig config;
            config.queue_system_id = get_trimmed_field(fields, queue_id_column);
            if (config.queue_system_id.empty())
            {
                throw runtime_error("Queue sys ID cannot be empty");
            }
            if (!seen_ids.insert(config.queue_system_id).second)
            {
                throw runtime_error("duplicate Queue sys ID: " + config.queue_system_id);
            }

            const long long parsed_max_queue_size =
                parse_required_integer(get_trimmed_field(fields, maximum_queue_size_column), "Max Queue Size");
            const long long parsed_num_servers =
                parse_required_integer(get_trimmed_field(fields, server_count_column), "# of servers");

            config.server_function_rate =
                parse_required_integer(get_trimmed_field(fields, server_function_rate_column), "Server Function Rate");

            if (parsed_max_queue_size < 0)
            {
                throw runtime_error("Max Queue Size cannot be negative for " + config.queue_system_id);
            }
            if (parsed_num_servers <= 0)
            {
                throw runtime_error("# of servers must be positive for " + config.queue_system_id);
            }
            if (config.server_function_rate <= 0)
            {
                throw runtime_error("Server Function Rate must be positive for " + config.queue_system_id);
            }

            config.maximum_queue_size = static_cast<size_t>(parsed_max_queue_size);
            config.number_of_servers = static_cast<size_t>(parsed_num_servers);

            config.server_function_text = get_trimmed_field(fields, server_function_column);
            config.server_function_chain = qsim::parse_server_function(config.server_function_text);

            config.source_workload_path =
                remove_matching_outer_quotes(get_trimmed_field(fields, source_column));
            config.sink_workload_path =
                remove_matching_outer_quotes(get_trimmed_field(fields, sink_column));
            config.incoming_edges = qsim::parse_edge_list(get_trimmed_field(fields, incoming_edges_column));
            config.outgoing_edges = qsim::parse_edge_list(get_trimmed_field(fields, outgoing_edges_column));

            multi_queue_config.queue_systems.push_back(std::move(config));
        }

        // Every edge must point at a queue-system that actually exists in this file.
        for (const QueueSystemConfig &config : multi_queue_config.queue_systems)
        {
            for (const string &edge_target : config.outgoing_edges)
            {
                if (!seen_ids.count(edge_target))
                {
                    throw runtime_error("queue-system " + config.queue_system_id +
                                        " has an outgoing edge to unknown queue-system: " + edge_target);
                }
            }
            for (const string &edge_source : config.incoming_edges)
            {
                if (!seen_ids.count(edge_source))
                {
                    throw runtime_error("queue-system " + config.queue_system_id +
                                        " has an incoming edge from unknown queue-system: " + edge_source);
                }
            }
        }

        return multi_queue_config;
    }

    // ----------------------------------------------------------------------
    // Public: run_multi_simulation
    // ----------------------------------------------------------------------

    SimulationResult run_multi_simulation(const filesystem::path &configuration_path,
                                          const SimulationOptions &simulation_options)
    {
        const MultiQueueConfig multi_queue_config = load_multi_queue_config(configuration_path);

        if (simulation_options.time_limit && *simulation_options.time_limit < 0)
        {
            throw runtime_error("time limit cannot be negative");
        }

        create_parent_directories_if_needed(simulation_options.events_output_path);
        create_parent_directories_if_needed(simulation_options.metrics_output_path);

        ofstream event_log_file(simulation_options.events_output_path, ios::trunc);
        ofstream metrics_output_file(simulation_options.metrics_output_path, ios::trunc);

        if (!event_log_file)
        {
            throw runtime_error("cannot write event log: " + simulation_options.events_output_path.string());
        }
        if (!metrics_output_file)
        {
            throw runtime_error("cannot write queue metrics: " + simulation_options.metrics_output_path.string());
        }

        metrics_output_file << "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,num-of-errors\n";

        // One runtime record per queue-system, keyed by id for O(1) routing lookups.
        vector<QueueSystemRuntime> runtimes(multi_queue_config.queue_systems.size());
        unordered_map<string, size_t> runtime_index_by_id;

        for (size_t i = 0; i < multi_queue_config.queue_systems.size(); ++i)
        {
            const QueueSystemConfig &config = multi_queue_config.queue_systems[i];
            runtimes[i].config = &config;
            runtimes[i].server_states.resize(config.number_of_servers);
            runtime_index_by_id[config.queue_system_id] = i;

            if (!config.source_workload_path.empty())
            {
                runtimes[i].source_workload_jobs = qsim::load_workload(
                    resolve_path_relative_to_configuration(configuration_path, config.source_workload_path));
            }
        }

        // Write one event line, tagged with which queue-system it happened in.
        auto write_event = [&](long long t, const QueueSystemConfig &config,
                               const qsim::Job &job, const string &action_text)
        {
            event_log_file << t << ' ' << job.job_id << " IN "
                           << config.queue_system_id << ' ' << action_text << '\n';
        };

        // Jobs arriving at a queue-system this tick, from either a source file
        // or another queue-system's outgoing edge. Rebuilt fresh every tick.
        vector<vector<qsim::Job>> pending_arrivals(runtimes.size());

        // Forward-declared because route_departing_job and admit_arriving_job
        // call each other (an empty job delivered across an edge can depart
        // again immediately).
        function<void(size_t, long long, qsim::Job)> route_departing_job;
        function<void(size_t, long long, qsim::Job)> admit_arriving_job;

        route_departing_job = [&](size_t queue_index, long long t, qsim::Job departing_job)
        {
            QueueSystemRuntime &runtime = runtimes[queue_index];
            const QueueSystemConfig &config = *runtime.config;

            if (departing_job.tasks.empty() && !config.sink_workload_path.empty())
            {
                departing_job.arrival_time_step = t;
                write_event(t, config, departing_job, "DEPART-VIA " + config.sink_workload_path);
                runtime.completed_sink_jobs.push_back(std::move(departing_job));
                return;
            }

            if (!config.outgoing_edges.empty())
            {
                const string &target_id =
                    config.outgoing_edges[runtime.next_outgoing_edge_index % config.outgoing_edges.size()];
                ++runtime.next_outgoing_edge_index;
                ++runtime.outgoing_edge_departure_count;

                write_event(t, config, departing_job, "DEPART-VIA " + target_id);

                // Deliver into the target queue-system's arrivals for this same tick.
                const size_t target_index = runtime_index_by_id.at(target_id);
                pending_arrivals[target_index].push_back(std::move(departing_job));
                return;
            }

            ++runtime.cumulative_error_count;
            write_event(t, config, departing_job, "ERROR NO-OUT-EDGE");
        };

        auto start_job_on_server = [&](size_t queue_index, size_t server_id,
                                       long long t, qsim::Job job_to_start)
        {
            QueueSystemRuntime &runtime = runtimes[queue_index];
            const QueueSystemConfig &config = *runtime.config;
            ServerRuntimeState &server = runtime.server_states.at(server_id);

            server.is_busy = true;
            server.current_job = std::move(job_to_start);
            server.service_time_remaining = config.server_function_rate;
            server.server_function_step_index = 0;

            write_event(t, config, server.current_job, "ENTERS-SERVER " + to_string(server_id));

            if (!server.current_job.tasks.empty())
            {
                const qsim::Task &first_task = server.current_job.tasks.front();
                write_event(t, config, server.current_job,
                            "TASK-START " + first_task.task_id + " \"" + format_task_body(first_task) + "\"");
            }
        };

        // Handles one job newly available to a queue-system this tick,
        // whether it came from that queue's own source file or from another
        // queue-system's outgoing edge. Mirrors Obj-B's arrival handling.
        admit_arriving_job = [&](size_t queue_index, long long t, qsim::Job arriving_job)
        {
            QueueSystemRuntime &runtime = runtimes[queue_index];
            const QueueSystemConfig &config = *runtime.config;

            if (arriving_job.tasks.empty())
            {
                route_departing_job(queue_index, t, std::move(arriving_job));
                return;
            }

            const auto first_idle_server =
                find_if(runtime.server_states.begin(), runtime.server_states.end(),
                        [](const ServerRuntimeState &s)
                        { return !s.is_busy; });

            if (runtime.waiting_jobs.empty() && first_idle_server != runtime.server_states.end())
            {
                const size_t idle_server_id =
                    static_cast<size_t>(distance(runtime.server_states.begin(), first_idle_server));
                start_job_on_server(queue_index, idle_server_id, t, std::move(arriving_job));
            }
            else if (runtime.waiting_jobs.size() < config.maximum_queue_size)
            {
                runtime.waiting_jobs.push_back(std::move(arriving_job));
            }
            else
            {
                ++runtime.cumulative_error_count;
                write_event(t, config, arriving_job, "ERROR QUEUE-FULL");
            }
        };

        auto dispatch_waiting_jobs_to_idle_servers = [&](size_t queue_index, long long t)
        {
            QueueSystemRuntime &runtime = runtimes[queue_index];
            for (size_t server_id = 0;
                 server_id < runtime.server_states.size() && !runtime.waiting_jobs.empty();
                 ++server_id)
            {
                if (!runtime.server_states[server_id].is_busy)
                {
                    qsim::Job next_job = std::move(runtime.waiting_jobs.front());
                    runtime.waiting_jobs.pop_front();
                    start_job_on_server(queue_index, server_id, t, std::move(next_job));
                }
            }
        };

        auto all_queue_systems_finished = [&]()
        {
            return all_of(runtimes.begin(), runtimes.end(),
                          [](const QueueSystemRuntime &r)
                          { return r.is_finished(); });
        };

        long long current_time_step = 0;

        auto simulation_should_continue = [&]()
        {
            if (simulation_options.time_limit)
            {
                return current_time_step < *simulation_options.time_limit;
            }
            return !all_queue_systems_finished();
        };

        while (simulation_should_continue())
        {
            for (auto &arrivals : pending_arrivals)
            {
                arrivals.clear();
            }

            // ----------------------------------------------------------
            // Phase 1: advance service, complete tasks, route finished jobs.
            // Departures headed for another queue-system land in
            // pending_arrivals for THIS tick (phase 4 below), so routing
            // never depends on which queue-system happens to be processed
            // first - every queue advances its servers before anyone's
            // arrivals are admitted.
            // ----------------------------------------------------------
            for (size_t queue_index = 0; queue_index < runtimes.size(); ++queue_index)
            {
                QueueSystemRuntime &runtime = runtimes[queue_index];
                const QueueSystemConfig &config = *runtime.config;

                for (size_t server_id = 0; server_id < runtime.server_states.size(); ++server_id)
                {
                    ServerRuntimeState &server = runtime.server_states[server_id];
                    if (!server.is_busy)
                    {
                        continue;
                    }

                    --server.service_time_remaining;
                    if (server.service_time_remaining > 0)
                    {
                        continue;
                    }

                    const qsim::Task task_before = server.current_job.tasks.front();
                    write_event(current_time_step, config, server.current_job,
                                "TASK-STOP " + task_before.task_id + " \"" + format_task_body(task_before) + "\"");

                    const long long subtraction_amount =
                        config.server_function_chain.subtraction_amounts.at(server.server_function_step_index);

                    if (server.current_job.tasks.front().remaining_work <= subtraction_amount)
                    {
                        server.current_job.tasks.erase(server.current_job.tasks.begin());
                    }
                    else
                    {
                        server.current_job.tasks.front().remaining_work -= subtraction_amount;
                    }

                    ++server.server_function_step_index;

                    bool continue_on_same_server = false;
                    if (!server.current_job.tasks.empty())
                    {
                        if (server.server_function_step_index <
                            config.server_function_chain.subtraction_amounts.size())
                        {
                            continue_on_same_server = true;
                        }
                        else if (config.server_function_chain.repeat_until_tasks_complete)
                        {
                            server.server_function_step_index = 0;
                            continue_on_same_server = true;
                        }
                    }

                    if (continue_on_same_server)
                    {
                        server.service_time_remaining = config.server_function_rate;
                        const qsim::Task &task_after = server.current_job.tasks.front();
                        write_event(current_time_step, config, server.current_job,
                                    "TASK-START " + task_after.task_id + " \"" + format_task_body(task_after) + "\"");
                    }
                    else
                    {
                        write_event(current_time_step, config, server.current_job,
                                    "EXITS-SERVER " + to_string(server_id));
                        qsim::Job departing_job = std::move(server.current_job);
                        server = ServerRuntimeState{};
                        route_departing_job(queue_index, current_time_step, std::move(departing_job));
                    }
                }
            }

            // ----------------------------------------------------------
            // Phase 2: dispatch waiting jobs into now-idle servers.
            // ----------------------------------------------------------
            for (size_t queue_index = 0; queue_index < runtimes.size(); ++queue_index)
            {
                dispatch_waiting_jobs_to_idle_servers(queue_index, current_time_step);
            }

            // ----------------------------------------------------------
            // Phase 3: admit this tick's arrivals - from each queue-system's
            // own source file, and from other queue-systems' departures
            // collected in phase 1.
            // ----------------------------------------------------------
            for (size_t queue_index = 0; queue_index < runtimes.size(); ++queue_index)
            {
                QueueSystemRuntime &runtime = runtimes[queue_index];
                const QueueSystemConfig &config = *runtime.config;

                while (runtime.next_source_job_index < runtime.source_workload_jobs.size() &&
                       runtime.source_workload_jobs[runtime.next_source_job_index].arrival_time_step ==
                           current_time_step)
                {
                    qsim::Job arriving_job = runtime.source_workload_jobs[runtime.next_source_job_index++];
                    write_event(current_time_step, config, arriving_job,
                                "ARRIVE-VIA " + config.source_workload_path);
                    admit_arriving_job(queue_index, current_time_step, std::move(arriving_job));
                }

                for (qsim::Job &arriving_job : pending_arrivals[queue_index])
                {
                    admit_arriving_job(queue_index, current_time_step, std::move(arriving_job));
                }
            }

            // A job delivered across an edge might land straight on an idle
            // server (phase 3 above already does that per queue-system), but
            // if it instead joined a wait queue, give it one more chance to
            // reach a server that just freed up this same tick before the
            // metrics row is written. This mirrors dispatching immediately
            // after admission, same as Obj-B does for source arrivals.
            for (size_t queue_index = 0; queue_index < runtimes.size(); ++queue_index)
            {
                dispatch_waiting_jobs_to_idle_servers(queue_index, current_time_step);
            }

            // ----------------------------------------------------------
            // Phase 4: write one metrics row per queue-system for this tick.
            // ----------------------------------------------------------
            for (size_t queue_index = 0; queue_index < runtimes.size(); ++queue_index)
            {
                QueueSystemRuntime &runtime = runtimes[queue_index];
                const QueueSystemConfig &config = *runtime.config;

                const size_t busy_server_count = static_cast<size_t>(
                    count_if(runtime.server_states.begin(), runtime.server_states.end(),
                             [](const ServerRuntimeState &s)
                             { return s.is_busy; }));

                metrics_output_file << current_time_step << ','
                                    << escape_csv_field(config.queue_system_id) << ','
                                    << runtime.waiting_jobs.size() << ','
                                    << busy_server_count << ','
                                    << runtime.cumulative_error_count << '\n';
            }

            ++current_time_step;
        }

        SimulationResult result;
        result.time_steps_processed = current_time_step;

        // Several queue-systems may name the same sink file (e.g. every exit
        // node of a generated network uses "out0.wl"). Collect their jobs per
        // resolved file first and write each file exactly once, so a later
        // queue-system cannot overwrite an earlier one's completed jobs.
        map<filesystem::path, vector<qsim::Job>> completed_jobs_by_sink_file;

        for (size_t queue_index = 0; queue_index < runtimes.size(); ++queue_index)
        {
            QueueSystemRuntime &runtime = runtimes[queue_index];
            const QueueSystemConfig &config = *runtime.config;

            if (!config.sink_workload_path.empty())
            {
                vector<qsim::Job> &sink_file_jobs = completed_jobs_by_sink_file[
                    resolve_path_relative_to_configuration(configuration_path, config.sink_workload_path)];
                sink_file_jobs.insert(sink_file_jobs.end(),
                                      runtime.completed_sink_jobs.begin(),
                                      runtime.completed_sink_jobs.end());
            }

            QueueSystemResult per_queue_result;
            per_queue_result.queue_system_id = config.queue_system_id;
            per_queue_result.jobs_written_to_sink = runtime.completed_sink_jobs.size();
            per_queue_result.jobs_departed_via_outgoing_edge = runtime.outgoing_edge_departure_count;
            per_queue_result.jobs_left_waiting = runtime.waiting_jobs.size();
            per_queue_result.jobs_left_in_service = static_cast<size_t>(
                count_if(runtime.server_states.begin(), runtime.server_states.end(),
                         [](const ServerRuntimeState &s)
                         { return s.is_busy; }));
            per_queue_result.source_jobs_not_yet_arrived =
                runtime.source_workload_jobs.size() - runtime.next_source_job_index;
            per_queue_result.errors = runtime.cumulative_error_count;

            result.total_errors += runtime.cumulative_error_count;
            result.per_queue_system.push_back(std::move(per_queue_result));
        }

        // Within one file, order jobs by the time they left the network;
        // jobs leaving in the same tick keep queue-system (config row) order.
        for (auto &[sink_file_path, sink_file_jobs] : completed_jobs_by_sink_file)
        {
            stable_sort(sink_file_jobs.begin(), sink_file_jobs.end(),
                        [](const qsim::Job &a, const qsim::Job &b)
                        { return a.arrival_time_step < b.arrival_time_step; });
            qsim::write_workload(sink_file_path, sink_file_jobs);
        }

        return result;
    }

} // namespace mqsim