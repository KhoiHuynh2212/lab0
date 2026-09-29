PYTHON ?= python3
PIP ?= $(PYTHON) -m pip

.PHONY: all build clean test check-deps

all: build

build: check-deps
	chmod +x plot-sys-metrics plot_sys_metrics.py run-all-tests.sh

check-deps:
	@$(PYTHON) -c "import matplotlib" \
		|| { echo "matplotlib is required. Try: $(PIP) install matplotlib" >&2; exit 1; }

test: build
	./run-all-tests.sh

clean:
	rm -rf .test-build
	rm -f *.png
	find . -type d -name '__pycache__' -exec rm -rf {} + 2>/dev/null || true
