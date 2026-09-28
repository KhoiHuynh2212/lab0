# Python plot-use tool. "build" checks dependencies and marks the launcher executable.

PYTHON ?= python3
PIP ?= $(PYTHON) -m pip

.PHONY: all build clean test check-deps

all: build

# Confirm matplotlib/pandas are available, then make ./plot-use runnable.
build: check-deps
	chmod +x plot-use plot_use.py run-all-tests.sh

check-deps:
	@$(PYTHON) -c "import matplotlib" \
		|| { echo "matplotlib is required. Try: $(PIP) install matplotlib" >&2; exit 1; }

# Run the Python test suite on the fixture CSV.
test: build
	./run-all-tests.sh

# Remove generated plots and cached bytecode.
clean:
	rm -rf results .test-build
	rm -f *.png
	find . -type d -name '__pycache__' -exec rm -rf {} + 2>/dev/null || true
