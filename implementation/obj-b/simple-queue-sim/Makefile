# C++ compiler used for the Obj-B simple queue simulator.
CXX ?= g++

# Valgrind command used by every test target.
VALGRIND ?= valgrind

# Normal build flags keep warnings enabled and use C++20.
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude

# Name of the simulator executable.
TARGET := simple-queue-sim

# Source files and public header used by the simulator.
SOURCE_FILES := src/main.cpp src/queue_sim.cpp
PUBLIC_HEADER := include/queue_sim.hpp

.PHONY: all build clean test run-tests-coverage run-tests-leak check-valgrind

# Typing only "make" builds the simulator.
all: build

# Start from a clean directory, then compile the simulator.
build: clean
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCE_FILES) -o $(TARGET)

# Stop before testing if Valgrind is not available.
check-valgrind:
	@command -v $(VALGRIND) >/dev/null 2>&1 || { echo "Valgrind is required to run tests." >&2; exit 1; }

# Run the complete test suite. Every executable test is run through Valgrind.
test: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh all

# Run the Valgrind test suite and create the gcov report for the simulator core.
run-tests-coverage: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh coverage

# Run the Valgrind test suite with leak checking enabled.
run-tests-leak: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh leak

# Remove executables and files created by builds, tests, logs, and gcov.
clean:
	rm -f $(TARGET)
	rm -rf .test-build
	rm -f *.gcda *.gcno *.gcov
	rm -f qmet.csv job.log
