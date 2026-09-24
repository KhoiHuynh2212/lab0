# Obj-A contains two C++ programs: the workload generator and workload visualizer.
GENERATOR_DIRECTORY := workload-generator
VISUALIZER_DIRECTORY := workload-visualizer

# Valgrind command passed to every Obj-A test script.
VALGRIND ?= valgrind

.PHONY: all build clean test run-tests-coverage run-tests-leak check-valgrind

# Typing only "make" builds both Obj-A programs.
all: build

# Clean old build files, then build both Obj-A programs.
build: clean
	$(MAKE) -C $(GENERATOR_DIRECTORY) build
	$(MAKE) -C $(VISUALIZER_DIRECTORY) build

# Stop before testing if Valgrind is not available.
check-valgrind:
	@command -v $(VALGRIND) >/dev/null 2>&1 || { echo "Valgrind is required to run tests." >&2; exit 1; }

# Run all Obj-A tests and the Obj-A to Obj-B compatibility test under Valgrind.
test: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh all

# Run the Obj-A tests under Valgrind and create the component gcov reports.
run-tests-coverage: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh coverage

# Run the Obj-A tests and compatibility check under Valgrind for leak checking.
run-tests-leak: build check-valgrind
	VALGRIND="$(VALGRIND)" ./run-all-tests.sh leak

# Remove all files created by the two Obj-A builds and their tests.
clean:
	$(MAKE) -C $(GENERATOR_DIRECTORY) clean
	$(MAKE) -C $(VISUALIZER_DIRECTORY) clean
