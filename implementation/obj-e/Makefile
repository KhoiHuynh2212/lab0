# The ONE Makefile for Obj-E. Run every command from this folder (obj-e/):
#   make                      build the simulator
#   make test                 C++ tests (Valgrind) + Python tool tests
#   make run-tests-coverage   gcov report + Python coverage
#   make run-tests-leak       C++ tests with full leak checking
#   make check-python         are the .py tools present, non-empty, valid?
#   make test-python          unit tests of pretty-print-events
#   make clean
#
# The executable is built INSIDE multi-queue-sim/ (multi-queue-sim/multi-queue-sim)
# so it never collides with the folder name.

CXX ?= g++
VALGRIND ?= valgrind
PYTHON ?= python3
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2

# Obj-B's include/ and src/. Relative paths are read from INSIDE multi-queue-sim/
# (that is where the compiler runs), e.g.:  make OBJB_DIR=../../obj-b
OBJB_DIR ?= ../../obj-b/simple-queue-sim
CPPFLAGS ?= -Iinclude -I$(OBJB_DIR)/include

MQ_DIR := multi-queue-sim
TARGET := multi-queue-sim
SOURCE_FILES := src/main.cpp src/multi_queue_sim.cpp $(OBJB_DIR)/src/queue_sim.cpp

# Python tools (paths relative to obj-e/).
PRETTY_PRINT_DIR := pretty-print-events
CREATE_CONFIG_DIR := create-config-from-network
PRETTY_PRINT_SCRIPT := $(PRETTY_PRINT_DIR)/pretty-print-events.py
PRETTY_PRINT_TEST := test_pretty_print_events.py
CREATE_CONFIG_SCRIPT := $(CREATE_CONFIG_DIR)/create-config-from-network.py

.PHONY: all build clean clean-cpp test run-tests-coverage run-tests-leak check-valgrind \
        check-python test-python run-tests-python-coverage

all: build

# Start from a clean state, then compile the simulator.
build: clean-cpp
	cd $(MQ_DIR) && $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCE_FILES) -o $(TARGET)

check-valgrind:
	@command -v $(VALGRIND) >/dev/null 2>&1 || { echo "Valgrind is required to run tests." >&2; exit 1; }

# C++ tests through Valgrind, then the Python tool tests.
test: build check-valgrind
	cd $(MQ_DIR) && OBJB_DIR="$(OBJB_DIR)" VALGRIND="$(VALGRIND)" ./run-all-tests.sh all
	@$(MAKE) test-python

# gcov report for the simulator core, then Python coverage.
run-tests-coverage: build check-valgrind
	cd $(MQ_DIR) && OBJB_DIR="$(OBJB_DIR)" VALGRIND="$(VALGRIND)" ./run-all-tests.sh coverage
	@$(MAKE) run-tests-python-coverage

run-tests-leak: build check-valgrind
	cd $(MQ_DIR) && OBJB_DIR="$(OBJB_DIR)" VALGRIND="$(VALGRIND)" ./run-all-tests.sh leak

# Make sure the Python tools exist, are not empty (an empty .py file runs
# "successfully" and prints nothing), and have valid Python syntax.
check-python:
	@for script in $(PRETTY_PRINT_SCRIPT) $(CREATE_CONFIG_SCRIPT); do \
	  test -s "$$script" || { echo "$$script is missing or empty" >&2; exit 1; }; \
	done
	@$(PYTHON) -c "import ast, sys; [ast.parse(open(p, encoding='utf-8').read(), p) for p in sys.argv[1:]]" \
	  $(PRETTY_PRINT_SCRIPT) $(CREATE_CONFIG_SCRIPT)
	@echo "Python tools OK: pretty-print-events.py, create-config-from-network.py"

# Unit tests for pretty-print-events (standard library only).
test-python: check-python
	cd $(PRETTY_PRINT_DIR) && $(PYTHON) $(PRETTY_PRINT_TEST)

# Line + branch coverage for pretty-print-events. Needs: pip install coverage
# If the coverage module is missing this prints a warning and skips (it does
# not fail the C++ coverage run).
run-tests-python-coverage: check-python
	@if $(PYTHON) -c "import coverage" 2>/dev/null; then \
	  cd $(PRETTY_PRINT_DIR) && \
	  $(PYTHON) -m coverage erase && \
	  $(PYTHON) -m coverage run --branch --include="pretty-print-events.py" $(PRETTY_PRINT_TEST) && \
	  $(PYTHON) -m coverage report -m; \
	else \
	  echo "WARNING: python 'coverage' is not installed (pip install coverage); skipping Python coverage." >&2; \
	fi

clean-cpp:
	cd $(MQ_DIR) && rm -f $(TARGET) && rm -rf .test-build && rm -f *.gcda *.gcno *.gcov && rm -f qmet.csv job.log

# Remove build/test output and Python caches.
clean: clean-cpp
	rm -rf $(PRETTY_PRINT_DIR)/__pycache__ $(PRETTY_PRINT_DIR)/.coverage
	rm -rf $(CREATE_CONFIG_DIR)/__pycache__