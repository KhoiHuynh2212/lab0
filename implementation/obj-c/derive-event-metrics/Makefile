CXX ?= g++
VALGRIND ?= valgrind
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?= -Iinclude

TARGET := derive-event-metrics
SOURCE_FILES := src/main.cpp src/derive_event_metrics.cpp

.PHONY: all build clean test run-tests-coverage run-tests-leak check-valgrind

all: build

build: clean
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCE_FILES) -o $(TARGET)

check-valgrind:
	@command -v $(VALGRIND) >/dev/null 2>&1 || { echo "Valgrind is required to run tests." >&2; exit 1; }

test: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh all

run-tests-coverage: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh coverage

run-tests-leak: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh leak

clean:
	rm -f $(TARGET)
	rm -rf .test-build
	rm -f *.gcda *.gcno *.gcov
	rm -f src/*.gcda src/*.gcno src/*.gcov
	rm -f tests/*.gcda tests/*.gcno tests/*.gcov
