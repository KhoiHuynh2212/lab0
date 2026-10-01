// Obj-B example server functions.

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

struct Task {
    std::string id;
    long long work_left;
};

// Example: Sub-X
// - If the first task has more than X work remaining, subtract X.
// - If it has X or less, remove that task.
// - An empty task list remains empty.
void sub_x(std::vector<Task>& tasks, long long x) {
    assert(x > 0);

    if (tasks.empty()) {
        return;
    }

    if (tasks.front().work_left <= x) {
        tasks.erase(tasks.begin());
    } else {
        tasks.front().work_left -= x;
    }
}

// Example chained server function: "Sub-5,Sub-10".
// The simulator would apply each element on a separate Server Function Rate
// interval. This helper performs one complete pass through the chain.
void apply_chain_once(std::vector<Task>& tasks,
                      const std::vector<long long>& subtraction_amounts) {
    for (long long amount : subtraction_amounts) {
        sub_x(tasks, amount);
    }
}

// Example repeating chain: "Sub-10*".
// The trailing '*' means repeat the chain until the job has no tasks left.
void apply_repeating_chain(std::vector<Task>& tasks,
                           const std::vector<long long>& subtraction_amounts) {
    while (!tasks.empty()) {
        apply_chain_once(tasks, subtraction_amounts);
    }
}

void print_tasks(const std::vector<Task>& tasks) {
    std::cout << '[';
    for (std::size_t i = 0; i < tasks.size(); ++i) {
        if (i != 0) {
            std::cout << ", ";
        }
        std::cout << '"' << tasks[i].id << ": W " << tasks[i].work_left << '"';
    }
    std::cout << "]\n";
}

int main() {
    // Sub-10(["0: W 10"]) -> []
    std::vector<Task> a{{"0", 10}};
    sub_x(a, 10);
    assert(a.empty());

    // Sub-10(["0: W 20"]) -> ["0: W 10"]
    std::vector<Task> b{{"0", 20}};
    sub_x(b, 10);
    assert(b.size() == 1 && b[0].work_left == 10);

    // Sub-10(["0: W 10", "1: W 10"]) -> ["1: W 10"]
    std::vector<Task> c{{"0", 10}, {"1", 10}};
    sub_x(c, 10);
    assert(c.size() == 1 && c[0].id == "1" && c[0].work_left == 10);

    // One pass of Sub-5,Sub-10 against W 20 leaves W 5.
    std::vector<Task> d{{"0", 20}};
    apply_chain_once(d, {5, 10});
    assert(d.size() == 1 && d[0].work_left == 5);

    // Repeating Sub-10* completes every remaining task.
    std::vector<Task> e{{"0", 20}, {"1", 10}};
    apply_repeating_chain(e, {10});
    assert(e.empty());

    std::cout << "server-function examples passed\n";
    print_tasks(e); // prints []
    return 0;
}
