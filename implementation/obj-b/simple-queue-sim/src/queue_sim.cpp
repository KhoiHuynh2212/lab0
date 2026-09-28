#include "queue_sim.hpp"

#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <utility>

using namespace std;

namespace qsim
{
    namespace
    {

        // Parsing and formatting helpers

        // Remove spaces, tabs, and line-ending characters from both ends of a string.
        string trim_whitespace(const string &input_text)
        {
            const size_t first_non_whitespace_position = input_text.find_first_not_of(" \t\r\n");
            if (first_non_whitespace_position == string::npos)
            {
                return {};
            }

            const size_t last_non_whitespace_position = input_text.find_last_not_of(" \t\r\n");
            return input_text.substr(first_non_whitespace_position,
                                     last_non_whitespace_position - first_non_whitespace_position + 1);
        }

        // Remove punctuation and capitalization differences from a CSV header.
        // For example, "Queue sys ID" and "queue-sys-id" become the same key.
        string make_lowercase_alphanumeric(const string &input_text)
        {
            string normalized_text;

            for (unsigned char current_character : input_text)
            {
                if (isalnum(current_character))
                {
                    normalized_text.push_back(static_cast<char>(tolower(current_character)));
                }
            }

            return normalized_text;
        }

        // Parse an integer field and report which field is invalid.
        long long parse_required_integer(const string &input_text, const string &field_name)
        {
            const string trimmed_value = trim_whitespace(input_text);
            if (trimmed_value.empty())
            {
                throw runtime_error("missing integer value for " + field_name);
            }

            size_t characters_consumed = 0;
            long long parsed_value = 0;

            try
            {
                parsed_value = stoll(trimmed_value, &characters_consumed);
            }
            catch (const exception &)
            {
                throw runtime_error("invalid integer for " + field_name + ": " + input_text);
            }

            // Make sure the entire field is a number, not just the beginning.
            if (characters_consumed != trimmed_value.size())
            {
                throw runtime_error("invalid integer for " + field_name + ": " + input_text);
            }

            return parsed_value;
        }

        // Remove one matching pair of single or double quotes around a value.
        string remove_matching_outer_quotes(const string &input_text)
        {
            string unquoted_text = trim_whitespace(input_text);

            const bool has_matching_single_quotes =
                unquoted_text.size() >= 2 && unquoted_text.front() == '\'' && unquoted_text.back() == '\'';
            const bool has_matching_double_quotes =
                unquoted_text.size() >= 2 && unquoted_text.front() == '"' && unquoted_text.back() == '"';

            if (has_matching_single_quotes || has_matching_double_quotes)
            {
                unquoted_text = unquoted_text.substr(1, unquoted_text.size() - 2);
            }

            return trim_whitespace(unquoted_text);
        }

        // Split list-style fields into separate items.
        // Items may use commas or semicolons and may be quoted.
        // Examples:
        //   []
        //   ['q1', 'q2']
        //   ["q1"; "q2"]
        //   Sub-5,Sub-10
        vector<string> split_list_items(const string &input_text)
        {
            string list_text = trim_whitespace(input_text);

            if (list_text.empty() || list_text == "[]")
            {
                return {};
            }

            if (list_text.front() == '[' && list_text.back() == ']')
            {
                list_text = trim_whitespace(list_text.substr(1, list_text.size() - 2));
            }

            if (list_text.empty())
            {
                return {};
            }

            vector<string> parsed_items;
            string current_item_text;
            char active_quote_character = '\0';

            for (size_t character_index = 0; character_index < list_text.size(); ++character_index)
            {
                const char current_character = list_text[character_index];

                if (active_quote_character != '\0')
                {
                    // Commas and semicolons inside quotes are part of the item.
                    // A backslash can escape the current quote character.
                    if (current_character == active_quote_character)
                    {
                        active_quote_character = '\0';
                    }
                    else if (current_character == '\\' &&
                             character_index + 1 < list_text.size() &&
                             list_text[character_index + 1] == active_quote_character)
                    {
                        current_item_text.push_back(list_text[++character_index]);
                    }
                    else
                    {
                        current_item_text.push_back(current_character);
                    }
                }
                else if (current_character == '\'' || current_character == '"')
                {
                    active_quote_character = current_character;
                }
                else if (current_character == ',' || current_character == ';')
                {
                    const string completed_item = trim_whitespace(current_item_text);
                    if (!completed_item.empty())
                    {
                        parsed_items.push_back(remove_matching_outer_quotes(completed_item));
                    }
                    current_item_text.clear();
                }
                else
                {
                    current_item_text.push_back(current_character);
                }
            }

            if (active_quote_character != '\0')
            {
                throw runtime_error("unterminated quote in list: " + input_text);
            }

            const string final_item = trim_whitespace(current_item_text);
            if (!final_item.empty())
            {
                parsed_items.push_back(remove_matching_outer_quotes(final_item));
            }

            return parsed_items;
        }

        // Add CSV quotes when a field contains commas, quotes, or line breaks.
        string escape_csv_field(const string &field_value)
        {
            const bool requires_quotes = field_value.find_first_of(",\"\r\n") != string::npos;
            if (!requires_quotes)
            {
                return field_value;
            }

            string escaped_field = "\"";
            for (char current_character : field_value)
            {
                // In CSV, a double quote inside a quoted field is written twice.
                if (current_character == '"')
                {
                    escaped_field += "\"\"";
                }
                else
                {
                    escaped_field.push_back(current_character);
                }
            }
            escaped_field.push_back('"');

            return escaped_field;
        }

        // Map each normalized CSV header name to its column number.
        map<string, size_t> build_normalized_header_index(const vector<string> &header_fields)
        {
            map<string, size_t> header_indexes;

            for (size_t column_index = 0; column_index < header_fields.size(); ++column_index)
            {
                header_indexes[make_lowercase_alphanumeric(header_fields[column_index])] = column_index;
            }

            return header_indexes;
        }

        // Find a required column using any accepted spelling of its name.
        size_t find_required_column_index(const map<string, size_t> &header_indexes,
                                          const vector<string> &accepted_normalized_names,
                                          const string &display_name)
        {
            for (const string &accepted_name : accepted_normalized_names)
            {
                const auto matching_header = header_indexes.find(accepted_name);
                if (matching_header != header_indexes.end())
                {
                    return matching_header->second;
                }
            }

            throw runtime_error("missing required column: " + display_name);
        }

        // Get one CSV field. Missing fields are treated as empty strings.
        string get_trimmed_field(const vector<string> &row_fields, size_t column_index)
        {
            return column_index < row_fields.size() ? trim_whitespace(row_fields[column_index]) : string{};
        }

        // Relative source and sink paths are based on the configuration file's folder.
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

        // Create any missing parent directories before opening an output file.
        void create_parent_directories_if_needed(const filesystem::path &output_path)
        {
            if (!output_path.parent_path().empty())
            {
                filesystem::create_directories(output_path.parent_path());
            }
        }

        // Format the work part of a task for the event log.
        string format_task_body(const Task &task)
        {
            return "W " + to_string(task.remaining_work);
        }

        // Current state of one server while the simulation is running.
        struct ServerRuntimeState
        {
            bool is_busy = false;
            Job current_job;
            long long service_time_remaining = 0;
            size_t server_function_step_index = 0;
        };

    } // namespace

    // Public parsing helpers

    vector<string> parse_csv_line(const string &csv_line)
    {
        vector<string> parsed_fields;
        string current_field;
        bool inside_quoted_field = false;

        for (size_t character_index = 0; character_index < csv_line.size(); ++character_index)
        {
            const char current_character = csv_line[character_index];

            if (inside_quoted_field)
            {
                if (current_character == '"')
                {
                    // Two double quotes inside a CSV field mean one literal quote.
                    if (character_index + 1 < csv_line.size() &&
                        csv_line[character_index + 1] == '"')
                    {
                        current_field.push_back('"');
                        ++character_index;
                    }
                    else
                    {
                        inside_quoted_field = false;
                    }
                }
                else
                {
                    current_field.push_back(current_character);
                }
            }
            else if (current_character == '"')
            {
                inside_quoted_field = true;
            }
            else if (current_character == ',')
            {
                parsed_fields.push_back(current_field);
                current_field.clear();
            }
            else if (current_character != '\r')
            {
                current_field.push_back(current_character);
            }
        }

        if (inside_quoted_field)
        {
            throw runtime_error("unterminated quoted CSV field");
        }

        parsed_fields.push_back(current_field);
        return parsed_fields;
    }

    vector<Task> parse_tasks(const string &task_list_text)
    {
        const vector<string> task_text_items = split_list_items(task_list_text);
        vector<Task> parsed_tasks;

        // A task must look like "<tid>: W N", where N is positive.
        const regex task_pattern(R"(^\s*([^:]+?)\s*:\s*[Ww]\s+([0-9]+)\s*$)");

        for (const string &task_text : task_text_items)
        {
            smatch regex_match_result;
            if (!regex_match(task_text, regex_match_result, task_pattern))
            {
                throw runtime_error("invalid task, expected '<tid>: W N': " + task_text);
            }

            const long long parsed_work_amount =
                parse_required_integer(regex_match_result[2].str(), "task work");

            if (parsed_work_amount <= 0)
            {
                throw runtime_error("task work must be positive: " + task_text);
            }

            parsed_tasks.push_back({trim_whitespace(regex_match_result[1].str()), parsed_work_amount});
        }

        return parsed_tasks;
    }

    string serialize_tasks(const vector<Task> &tasks)
    {
        if (tasks.empty())
        {
            return "[]";
        }

        ostringstream serialized_task_list;
        serialized_task_list << '[';

        for (size_t task_index = 0; task_index < tasks.size(); ++task_index)
        {
            if (task_index != 0)
            {
                serialized_task_list << ", ";
            }

            serialized_task_list << '\'' << tasks[task_index].task_id << ": W "
                                 << tasks[task_index].remaining_work << '\'';
        }

        serialized_task_list << ']';
        return serialized_task_list.str();
    }

    ServerFunctionChain parse_server_function(const string &server_function_text)
    {
        string normalized_function_text = trim_whitespace(server_function_text);
        if (normalized_function_text.empty())
        {
            throw runtime_error("server function cannot be empty");
        }

        ServerFunctionChain parsed_function_chain;

        // A trailing '*' repeats the chain until the job has no tasks left.
        if (normalized_function_text.back() == '*')
        {
            parsed_function_chain.repeat_until_tasks_complete = true;
            normalized_function_text =
                trim_whitespace(normalized_function_text.substr(0, normalized_function_text.size() - 1));
        }

        const vector<string> function_steps = split_list_items(normalized_function_text);
        if (function_steps.empty())
        {
            throw runtime_error("server function cannot be empty");
        }

        const regex subtraction_function_pattern(R"(^\s*[Ss][Uu][Bb]\s*-\s*([0-9]+)\s*$)");

        for (const string &function_step_text : function_steps)
        {
            smatch regex_match_result;
            if (!regex_match(function_step_text, regex_match_result, subtraction_function_pattern))
            {
                throw runtime_error("unsupported server function: " + function_step_text);
            }

            const long long subtraction_amount =
                parse_required_integer(regex_match_result[1].str(), "Sub-X amount");

            if (subtraction_amount <= 0)
            {
                throw runtime_error("Sub-X amount must be positive: " + function_step_text);
            }

            parsed_function_chain.subtraction_amounts.push_back(subtraction_amount);
        }

        return parsed_function_chain;
    }

    vector<string> parse_edge_list(const string &edge_list_text)
    {
        return split_list_items(edge_list_text);
    }

    // Configuration and workload file I/O

    QueueConfig load_queue_config(const filesystem::path &configuration_path)
    {
        ifstream configuration_file(configuration_path);
        if (!configuration_file)
        {
            throw runtime_error("cannot open queue configuration: " + configuration_path.string());
        }

        string current_line;
        if (!getline(configuration_file, current_line))
        {
            throw runtime_error("queue configuration is empty: " + configuration_path.string());
        }

        const vector<string> header_fields = parse_csv_line(current_line);
        const map<string, size_t> header_indexes = build_normalized_header_index(header_fields);

        // Accept the column names from the handout and a few simple spelling forms.
        const size_t queue_id_column =
            find_required_column_index(header_indexes,
                                       {"queuesysid", "queuesystemid", "qsid"},
                                       "Queue sys ID");
        const size_t maximum_queue_size_column =
            find_required_column_index(header_indexes,
                                       {"maxqueuesize", "queuesize"},
                                       "Max Queue Size");
        const size_t server_count_column =
            find_required_column_index(header_indexes,
                                       {"ofservers", "numservers", "numberofservers", "servers"},
                                       "# of servers");
        const size_t server_function_column =
            find_required_column_index(header_indexes,
                                       {"serverfunction", "function"},
                                       "Server Function");
        const size_t server_function_rate_column =
            find_required_column_index(header_indexes,
                                       {"serverfunctionrate", "functionrate", "rate"},
                                       "Server Function Rate");
        const size_t source_column =
            find_required_column_index(header_indexes, {"source"}, "source");
        const size_t sink_column =
            find_required_column_index(header_indexes, {"sink"}, "sink");
        const size_t incoming_edges_column =
            find_required_column_index(header_indexes,
                                       {"edgesin", "incomingedges"},
                                       "Edges in");
        const size_t outgoing_edges_column =
            find_required_column_index(header_indexes,
                                       {"edgesout", "outgoingedges"},
                                       "Edges out");

        vector<string> nonempty_configuration_rows;
        while (getline(configuration_file, current_line))
        {
            if (!trim_whitespace(current_line).empty())
            {
                nonempty_configuration_rows.push_back(current_line);
            }
        }

        // Obj-B uses one queue-system row.
        if (nonempty_configuration_rows.size() != 1)
        {
            throw runtime_error("simple queue configuration must contain exactly one queue-system row");
        }

        const vector<string> configuration_fields =
            parse_csv_line(nonempty_configuration_rows.front());

        QueueConfig queue_configuration;
        queue_configuration.queue_system_id =
            get_trimmed_field(configuration_fields, queue_id_column);

        if (queue_configuration.queue_system_id.empty())
        {
            throw runtime_error("Queue sys ID cannot be empty");
        }

        const long long parsed_maximum_queue_size =
            parse_required_integer(get_trimmed_field(configuration_fields, maximum_queue_size_column),
                                   "Max Queue Size");
        const long long parsed_number_of_servers =
            parse_required_integer(get_trimmed_field(configuration_fields, server_count_column),
                                   "# of servers");

        queue_configuration.server_function_rate =
            parse_required_integer(
                get_trimmed_field(configuration_fields, server_function_rate_column),
                "Server Function Rate");

        if (parsed_maximum_queue_size < 0)
        {
            throw runtime_error("Max Queue Size cannot be negative");
        }
        if (parsed_number_of_servers <= 0)
        {
            throw runtime_error("# of servers must be positive");
        }
        if (queue_configuration.server_function_rate <= 0)
        {
            throw runtime_error("Server Function Rate must be positive");
        }

        queue_configuration.maximum_queue_size =
            static_cast<size_t>(parsed_maximum_queue_size);
        queue_configuration.number_of_servers =
            static_cast<size_t>(parsed_number_of_servers);

        queue_configuration.server_function_text =
            get_trimmed_field(configuration_fields, server_function_column);
        queue_configuration.server_function_chain =
            parse_server_function(queue_configuration.server_function_text);

        queue_configuration.source_workload_path =
            remove_matching_outer_quotes(get_trimmed_field(configuration_fields, source_column));
        queue_configuration.sink_workload_path =
            remove_matching_outer_quotes(get_trimmed_field(configuration_fields, sink_column));
        queue_configuration.incoming_edges =
            parse_edge_list(get_trimmed_field(configuration_fields, incoming_edges_column));
        queue_configuration.outgoing_edges =
            parse_edge_list(get_trimmed_field(configuration_fields, outgoing_edges_column));

        return queue_configuration;
    }

    vector<Job> load_workload(const filesystem::path &workload_path)
    {
        ifstream workload_file(workload_path);
        if (!workload_file)
        {
            throw runtime_error("cannot open workload: " + workload_path.string());
        }

        string current_line;
        if (!getline(workload_file, current_line))
        {
            throw runtime_error("workload is empty: " + workload_path.string());
        }

        const vector<string> header_fields = parse_csv_line(current_line);
        const map<string, size_t> header_indexes = build_normalized_header_index(header_fields);

        const size_t job_id_column =
            find_required_column_index(header_indexes, {"jobid", "id"}, "Job Id");
        const size_t arrival_time_column =
            find_required_column_index(header_indexes,
                                       {"arrivaltime", "arrival"},
                                       "arrival time");
        const size_t task_list_column =
            find_required_column_index(header_indexes, {"tasks", "tasklist"}, "Tasks");

        // Keep the original row number so jobs with the same arrival time stay in file order.
        size_t original_input_order = 0;
        vector<pair<size_t, Job>> jobs_with_original_order;

        while (getline(workload_file, current_line))
        {
            if (trim_whitespace(current_line).empty())
            {
                continue;
            }

            const vector<string> workload_fields = parse_csv_line(current_line);
            Job parsed_job;

            parsed_job.job_id = get_trimmed_field(workload_fields, job_id_column);
            if (parsed_job.job_id.empty())
            {
                throw runtime_error("Job Id cannot be empty");
            }

            parsed_job.arrival_time_step =
                parse_required_integer(get_trimmed_field(workload_fields, arrival_time_column),
                                       "arrival time");

            if (parsed_job.arrival_time_step < 0)
            {
                throw runtime_error("arrival time cannot be negative for job " + parsed_job.job_id);
            }

            parsed_job.tasks = parse_tasks(get_trimmed_field(workload_fields, task_list_column));

            jobs_with_original_order.push_back(
                {original_input_order++, std::move(parsed_job)});
        }

        // Sort by arrival time while keeping file order for ties.
        stable_sort(jobs_with_original_order.begin(),
                    jobs_with_original_order.end(),
                    [](const auto &left_job_entry, const auto &right_job_entry)
                    {
                        return left_job_entry.second.arrival_time_step <
                               right_job_entry.second.arrival_time_step;
                    });

        vector<Job> sorted_jobs;
        sorted_jobs.reserve(jobs_with_original_order.size());

        for (auto &[ignored_original_order, sorted_job] : jobs_with_original_order)
        {
            (void)ignored_original_order;
            sorted_jobs.push_back(std::move(sorted_job));
        }

        return sorted_jobs;
    }

    void write_workload(const filesystem::path &workload_path, const vector<Job> &jobs)
    {
        create_parent_directories_if_needed(workload_path);

        ofstream workload_output_file(workload_path, ios::trunc);
        if (!workload_output_file)
        {
            throw runtime_error("cannot write workload: " + workload_path.string());
        }

        workload_output_file << "Job Id,arrival time,Tasks\n";

        for (const Job &job : jobs)
        {
            workload_output_file << escape_csv_field(job.job_id) << ','
                                 << job.arrival_time_step << ','
                                 << escape_csv_field(serialize_tasks(job.tasks)) << '\n';
        }
    }

    // Discrete-time simple queue simulation

    SimulationResult run_simulation(const filesystem::path &configuration_path,
                                    const SimulationOptions &simulation_options)
    {
        const QueueConfig queue_configuration = load_queue_config(configuration_path);

        vector<Job> source_workload_jobs;
        if (!queue_configuration.source_workload_path.empty())
        {
            source_workload_jobs = load_workload(
                resolve_path_relative_to_configuration(
                    configuration_path,
                    queue_configuration.source_workload_path));
        }

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
            throw runtime_error("cannot write event log: " +
                                simulation_options.events_output_path.string());
        }
        if (!metrics_output_file)
        {
            throw runtime_error("cannot write queue metrics: " +
                                simulation_options.metrics_output_path.string());
        }

        metrics_output_file
            << "time-step,queue-sys-id,num-jobs-in-queue,num-jobs-in-service,num-of-errors\n";

        // Jobs waiting for a server, in FIFO order.
        deque<Job> waiting_jobs;

        // Current state of each server.
        vector<ServerRuntimeState> server_states(queue_configuration.number_of_servers);

        // Completed jobs are saved here until the sink file is written.
        vector<Job> completed_sink_jobs;

        size_t next_source_job_index = 0;
        size_t cumulative_error_count = 0;
        size_t outgoing_edge_departure_count = 0;
        size_t next_outgoing_edge_index = 0;

        // Write one event using the required event-log format.
        auto write_event = [&](long long current_time_step,
                               const Job &job,
                               const string &action_text)
        {
            event_log_file << current_time_step << ' '
                           << job.job_id << " IN "
                           << queue_configuration.queue_system_id << ' '
                           << action_text << '\n';
        };

        // Send a departing job to the sink or an outgoing edge.
        // If neither route is available, record NO-OUT-EDGE.
        auto route_departing_job = [&](long long current_time_step, Job departing_job)
        {
            if (departing_job.tasks.empty() &&
                !queue_configuration.sink_workload_path.empty())
            {

                // Sink files store the departure time in the arrival-time column.
                departing_job.arrival_time_step = current_time_step;
                completed_sink_jobs.push_back(departing_job);

                write_event(current_time_step,
                            departing_job,
                            "DEPART-VIA " + queue_configuration.sink_workload_path);
                return;
            }

            if (!queue_configuration.outgoing_edges.empty())
            {
                // With multiple outgoing edges, take turns using them in round-robin order.
                const string &selected_outgoing_edge =
                    queue_configuration.outgoing_edges[next_outgoing_edge_index % queue_configuration.outgoing_edges.size()];

                ++next_outgoing_edge_index;
                ++outgoing_edge_departure_count;

                write_event(current_time_step,
                            departing_job,
                            "DEPART-VIA " + selected_outgoing_edge);
                return;
            }

            ++cumulative_error_count;
            write_event(current_time_step, departing_job, "ERROR NO-OUT-EDGE");
        };

        // Put a job on an idle server and start its first server-function step.
        auto start_job_on_server = [&](size_t server_id,
                                       long long current_time_step,
                                       Job job_to_start)
        {
            ServerRuntimeState &selected_server = server_states.at(server_id);

            selected_server.is_busy = true;
            selected_server.current_job = std::move(job_to_start);
            selected_server.service_time_remaining = queue_configuration.server_function_rate;
            selected_server.server_function_step_index = 0;

            write_event(current_time_step,
                        selected_server.current_job,
                        "ENTERS-SERVER " + to_string(server_id));

            // Jobs with no tasks are normally routed before this point.
            if (!selected_server.current_job.tasks.empty())
            {
                const Task &first_task = selected_server.current_job.tasks.front();
                write_event(current_time_step,
                            selected_server.current_job,
                            "TASK-START " + first_task.task_id + " \"" +
                                format_task_body(first_task) + "\"");
            }
        };

        // Fill idle servers from the front of the FIFO waiting queue.
        auto dispatch_waiting_jobs_to_idle_servers = [&](long long current_time_step)
        {
            for (size_t server_id = 0;
                 server_id < server_states.size() && !waiting_jobs.empty();
                 ++server_id)
            {

                if (!server_states[server_id].is_busy)
                {
                    Job next_waiting_job = std::move(waiting_jobs.front());
                    waiting_jobs.pop_front();

                    start_job_on_server(server_id,
                                        current_time_step,
                                        std::move(next_waiting_job));
                }
            }
        };

        auto any_server_is_busy = [&]()
        {
            return any_of(server_states.begin(),
                          server_states.end(),
                          [](const ServerRuntimeState &server_state)
                          {
                              return server_state.is_busy;
                          });
        };

        long long current_time_step = 0;

        auto simulation_should_continue = [&]()
        {
            if (simulation_options.time_limit)
            {
                return current_time_step < *simulation_options.time_limit;
            }

            // Without a time limit, stop when no source, queued, or server work remains.
            return next_source_job_index < source_workload_jobs.size() ||
                   !waiting_jobs.empty() ||
                   any_server_is_busy();
        };

        while (simulation_should_continue())
        {
            // ------------------------------------------------------------------
            // 1) Advance work already in service.
            // ------------------------------------------------------------------
            // A job that starts at time t begins counting down at the next time step.
            for (size_t server_id = 0; server_id < server_states.size(); ++server_id)
            {
                ServerRuntimeState &server_state = server_states[server_id];

                if (!server_state.is_busy)
                {
                    continue;
                }

                --server_state.service_time_remaining;
                if (server_state.service_time_remaining > 0)
                {
                    continue;
                }

                // The current server-function step is finished.
                // Log the task before changing its remaining work.
                const Task task_before_server_function =
                    server_state.current_job.tasks.front();

                write_event(current_time_step,
                            server_state.current_job,
                            "TASK-STOP " + task_before_server_function.task_id + " \"" +
                                format_task_body(task_before_server_function) + "\"");

                const long long subtraction_amount =
                    queue_configuration.server_function_chain.subtraction_amounts.at(
                        server_state.server_function_step_index);

                // Sub-X removes a finished first task or subtracts X from it.
                if (server_state.current_job.tasks.front().remaining_work <= subtraction_amount)
                {
                    server_state.current_job.tasks.erase(server_state.current_job.tasks.begin());
                }
                else
                {
                    server_state.current_job.tasks.front().remaining_work -= subtraction_amount;
                }

                ++server_state.server_function_step_index;

                bool should_continue_service_on_same_server = false;

                if (!server_state.current_job.tasks.empty())
                {
                    if (server_state.server_function_step_index <
                        queue_configuration.server_function_chain.subtraction_amounts.size())
                    {
                        // Continue with the next function in the chain.
                        should_continue_service_on_same_server = true;
                    }
                    else if (queue_configuration.server_function_chain
                                 .repeat_until_tasks_complete)
                    {
                        // '*' restarts the function chain.
                        server_state.server_function_step_index = 0;
                        should_continue_service_on_same_server = true;
                    }
                }

                if (should_continue_service_on_same_server)
                {
                    server_state.service_time_remaining = queue_configuration.server_function_rate;

                    const Task &task_after_server_function =
                        server_state.current_job.tasks.front();

                    write_event(current_time_step,
                                server_state.current_job,
                                "TASK-START " + task_after_server_function.task_id + " \"" +
                                    format_task_body(task_after_server_function) + "\"");
                }
                else
                {
                    // The job is done with this server, so remove and route it.
                    write_event(current_time_step,
                                server_state.current_job,
                                "EXITS-SERVER " + to_string(server_id));

                    Job departing_job = std::move(server_state.current_job);
                    server_state = ServerRuntimeState{};

                    route_departing_job(current_time_step, std::move(departing_job));
                }
            }

            // ------------------------------------------------------------------
            // 2) Move waiting jobs into idle servers.
            // ------------------------------------------------------------------
            dispatch_waiting_jobs_to_idle_servers(current_time_step);

            // ------------------------------------------------------------------
            // 3) Add source jobs that arrive at this time step.
            // ------------------------------------------------------------------
            // Use a while-loop because several jobs may arrive at the same time.
            while (next_source_job_index < source_workload_jobs.size() &&
                   source_workload_jobs[next_source_job_index].arrival_time_step ==
                       current_time_step)
            {

                Job arriving_job = source_workload_jobs[next_source_job_index++];

                write_event(current_time_step,
                            arriving_job,
                            "ARRIVE-VIA " + queue_configuration.source_workload_path);

                // A job with no tasks can leave immediately.
                if (arriving_job.tasks.empty())
                {
                    route_departing_job(current_time_step, std::move(arriving_job));
                    continue;
                }

                const auto first_idle_server =
                    find_if(server_states.begin(),
                            server_states.end(),
                            [](const ServerRuntimeState &server_state)
                            {
                                return !server_state.is_busy;
                            });

                if (waiting_jobs.empty() && first_idle_server != server_states.end())
                {
                    // A new job can enter an idle server immediately when the queue is empty.
                    const size_t idle_server_id = static_cast<size_t>(
                        distance(server_states.begin(), first_idle_server));

                    start_job_on_server(idle_server_id,
                                        current_time_step,
                                        std::move(arriving_job));
                }
                else if (waiting_jobs.size() < queue_configuration.maximum_queue_size)
                {
                    waiting_jobs.push_back(std::move(arriving_job));
                }
                else
                {
                    // The queue-size limit counts only waiting jobs.
                    ++cumulative_error_count;
                    write_event(current_time_step, arriving_job, "ERROR QUEUE-FULL");
                }
            }

            // ------------------------------------------------------------------
            // 4) Write one queue-metrics row for this time step.
            // ------------------------------------------------------------------
            const size_t busy_server_count = static_cast<size_t>(
                count_if(server_states.begin(),
                         server_states.end(),
                         [](const ServerRuntimeState &server_state)
                         {
                             return server_state.is_busy;
                         }));

            metrics_output_file << current_time_step << ','
                                << escape_csv_field(queue_configuration.queue_system_id) << ','
                                << waiting_jobs.size() << ','
                                << busy_server_count << ','
                                << cumulative_error_count << '\n';

            ++current_time_step;
        }

        // Write all completed jobs to the sink after the simulation ends.
        if (!queue_configuration.sink_workload_path.empty())
        {
            write_workload(
                resolve_path_relative_to_configuration(
                    configuration_path,
                    queue_configuration.sink_workload_path),
                completed_sink_jobs);
        }

        SimulationResult simulation_result;
        simulation_result.time_steps_processed = current_time_step;
        simulation_result.total_errors = cumulative_error_count;
        simulation_result.jobs_written_to_sink = completed_sink_jobs.size();
        simulation_result.jobs_departed_via_outgoing_edge = outgoing_edge_departure_count;
        simulation_result.jobs_left_waiting = waiting_jobs.size();
        simulation_result.jobs_left_in_service = static_cast<size_t>(
            count_if(server_states.begin(),
                     server_states.end(),
                     [](const ServerRuntimeState &server_state)
                     {
                         return server_state.is_busy;
                     }));
        simulation_result.source_jobs_not_yet_arrived =
            source_workload_jobs.size() - next_source_job_index;

        return simulation_result;
    }

} // namespace qsim
