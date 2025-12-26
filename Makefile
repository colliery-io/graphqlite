# GraphQLite Makefile

CC = gcc
BISON ?= bison
FLEX ?= flex
SQLITE ?= sqlite3
# Use python3.11 as default since it has pip available on this system
# Override with: make PYTHON=python3.12 test-python
PYTHON ?= python3.11

# Platform-specific paths for test builds (CUnit headers/libs)
# macOS with MacPorts: make EXTRA_LIBS=-L/opt/local/lib EXTRA_INCLUDES=-I/opt/local/include
# macOS with Homebrew: make EXTRA_INCLUDES=-I$(brew --prefix)/include EXTRA_LIBS=-L$(brew --prefix)/lib
EXTRA_LIBS ?=
EXTRA_INCLUDES ?=

# Vendored SQLite headers for consistent extension builds
VENDOR_SQLITE_DIR = bindings/python/vendor/sqlite

# Base flags for extension builds (vendor headers only, no system includes)
EXTENSION_BASE_CFLAGS = -Wall -Wextra -I$(VENDOR_SQLITE_DIR) -I./src/include

# Build mode: debug (default) or release
# Use: make extension RELEASE=1
ifdef RELEASE
CFLAGS = -Wall -Wextra -O2 -I$(VENDOR_SQLITE_DIR) -I./src/include $(EXTRA_INCLUDES)
EXTENSION_CFLAGS_BASE = -Wall -Wextra -O2 -I$(VENDOR_SQLITE_DIR) -I./src/include
else
# Add -DGRAPHQLITE_PERF_TIMING for detailed query timing instrumentation
CFLAGS = -Wall -Wextra -g -I$(VENDOR_SQLITE_DIR) -I./src/include -DGRAPHQLITE_DEBUG $(EXTRA_INCLUDES)
EXTENSION_CFLAGS_BASE = -Wall -Wextra -g -I$(VENDOR_SQLITE_DIR) -I./src/include -DGRAPHQLITE_DEBUG
endif
LDFLAGS = $(EXTRA_LIBS) -lcunit -lsqlite3 -lm

# Extension-specific flags: enable sqlite3ext.h API pointer redirection
EXTENSION_CFLAGS = -DGRAPHQLITE_EXTENSION

# Coverage flags
COVERAGE_FLAGS = -fprofile-arcs -ftest-coverage

# Detect OS and set coverage libs accordingly
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    COVERAGE_LIBS = -lgcov
endif
ifeq ($(UNAME_S),Darwin)
    # macOS with clang doesn't need -lgcov
    COVERAGE_LIBS =
endif

# Source directories
SRC_DIR = src
BACKEND_DIR = $(SRC_DIR)/backend
PARSER_DIR = $(BACKEND_DIR)/parser
TEST_DIR = tests

# Build directories
BUILD_DIR = build
BUILD_PARSER_DIR = $(BUILD_DIR)/parser
BUILD_TRANSFORM_DIR = $(BUILD_DIR)/transform
BUILD_EXECUTOR_DIR = $(BUILD_DIR)/executor
BUILD_TEST_DIR = $(BUILD_DIR)/tests
COVERAGE_DIR = $(BUILD_DIR)/coverage

# Parser sources (C files)
PARSER_SRCS = \
	$(PARSER_DIR)/cypher_keywords.c \
	$(PARSER_DIR)/cypher_scanner_api.c \
	$(PARSER_DIR)/cypher_ast.c \
	$(PARSER_DIR)/cypher_parser.c

# Generated sources
SCANNER_SRC = $(BUILD_PARSER_DIR)/cypher_scanner.c
SCANNER_L = $(PARSER_DIR)/cypher_scanner.l
GRAMMAR_SRC = $(BUILD_PARSER_DIR)/cypher_gram.tab.c
GRAMMAR_HDR = $(BUILD_PARSER_DIR)/cypher_gram.tab.h
GRAMMAR_Y = $(PARSER_DIR)/cypher_gram.y

# All parser sources including generated
ALL_PARSER_SRCS = $(PARSER_SRCS) $(SCANNER_SRC) $(GRAMMAR_SRC)

PARSER_OBJS = $(PARSER_SRCS:$(PARSER_DIR)/%.c=$(BUILD_PARSER_DIR)/%.o) $(BUILD_PARSER_DIR)/cypher_scanner.o $(BUILD_PARSER_DIR)/cypher_gram.tab.o
PARSER_OBJS_COV = $(PARSER_SRCS:$(PARSER_DIR)/%.c=$(BUILD_PARSER_DIR)/%.cov.o) $(BUILD_PARSER_DIR)/cypher_scanner.cov.o $(BUILD_PARSER_DIR)/cypher_gram.tab.cov.o
PARSER_OBJS_PIC = $(PARSER_SRCS:$(PARSER_DIR)/%.c=$(BUILD_PARSER_DIR)/%.pic.o) $(BUILD_PARSER_DIR)/cypher_scanner.pic.o $(BUILD_PARSER_DIR)/cypher_gram.tab.pic.o

# Transform sources
TRANSFORM_DIR = $(SRC_DIR)/backend/transform
TRANSFORM_SRCS = \
	$(TRANSFORM_DIR)/cypher_transform.c \
	$(TRANSFORM_DIR)/transform_match.c \
	$(TRANSFORM_DIR)/transform_create.c \
	$(TRANSFORM_DIR)/transform_set.c \
	$(TRANSFORM_DIR)/transform_delete.c \
	$(TRANSFORM_DIR)/transform_remove.c \
	$(TRANSFORM_DIR)/transform_foreach.c \
	$(TRANSFORM_DIR)/transform_load_csv.c \
	$(TRANSFORM_DIR)/transform_return.c \
	$(TRANSFORM_DIR)/transform_func_string.c \
	$(TRANSFORM_DIR)/transform_func_math.c \
	$(TRANSFORM_DIR)/transform_func_entity.c \
	$(TRANSFORM_DIR)/transform_func_path.c \
	$(TRANSFORM_DIR)/transform_func_list.c \
	$(TRANSFORM_DIR)/transform_func_graph.c \
	$(TRANSFORM_DIR)/transform_func_aggregate.c \
	$(TRANSFORM_DIR)/transform_func_dispatch.c \
	$(TRANSFORM_DIR)/transform_helpers.c \
	$(TRANSFORM_DIR)/transform_variables.c \
	$(TRANSFORM_DIR)/transform_expr_predicate.c \
	$(TRANSFORM_DIR)/transform_with.c \
	$(TRANSFORM_DIR)/transform_unwind.c \
	$(TRANSFORM_DIR)/transform_expr_ops.c

# Executor sources
EXECUTOR_DIR = $(SRC_DIR)/backend/executor
EXECUTOR_SRCS = \
	$(EXECUTOR_DIR)/cypher_schema.c \
	$(EXECUTOR_DIR)/cypher_executor.c \
	$(EXECUTOR_DIR)/executor_variable_map.c \
	$(EXECUTOR_DIR)/executor_foreach_ctx.c \
	$(EXECUTOR_DIR)/executor_result.c \
	$(EXECUTOR_DIR)/executor_helpers.c \
	$(EXECUTOR_DIR)/executor_delete.c \
	$(EXECUTOR_DIR)/executor_set.c \
	$(EXECUTOR_DIR)/executor_remove.c \
	$(EXECUTOR_DIR)/executor_create.c \
	$(EXECUTOR_DIR)/executor_foreach.c \
	$(EXECUTOR_DIR)/executor_merge.c \
	$(EXECUTOR_DIR)/executor_match.c \
	$(EXECUTOR_DIR)/agtype.c \
	$(EXECUTOR_DIR)/graph_algorithms.c \
	$(EXECUTOR_DIR)/graph_algo_pagerank.c \
	$(EXECUTOR_DIR)/graph_algo_community.c \
	$(EXECUTOR_DIR)/graph_algo_paths.c \
	$(EXECUTOR_DIR)/graph_algo_centrality.c \
	$(EXECUTOR_DIR)/graph_algo_components.c \
	$(EXECUTOR_DIR)/graph_algo_betweenness.c \
	$(EXECUTOR_DIR)/graph_algo_closeness.c \
	$(EXECUTOR_DIR)/graph_algo_louvain.c \
	$(EXECUTOR_DIR)/graph_algo_triangle.c \
	$(EXECUTOR_DIR)/graph_algo_astar.c \
	$(EXECUTOR_DIR)/graph_algo_traversal.c \
	$(EXECUTOR_DIR)/graph_algo_similarity.c \
	$(EXECUTOR_DIR)/graph_algo_knn.c \
	$(EXECUTOR_DIR)/graph_algo_eigenvector.c \
	$(EXECUTOR_DIR)/graph_algo_apsp.c

TRANSFORM_OBJS = $(TRANSFORM_SRCS:$(TRANSFORM_DIR)/%.c=$(BUILD_TRANSFORM_DIR)/%.o)
TRANSFORM_OBJS_COV = $(TRANSFORM_SRCS:$(TRANSFORM_DIR)/%.c=$(BUILD_TRANSFORM_DIR)/%.cov.o)
TRANSFORM_OBJS_PIC = $(TRANSFORM_SRCS:$(TRANSFORM_DIR)/%.c=$(BUILD_TRANSFORM_DIR)/%.pic.o)

EXECUTOR_OBJS = $(EXECUTOR_SRCS:$(EXECUTOR_DIR)/%.c=$(BUILD_EXECUTOR_DIR)/%.o)
EXECUTOR_OBJS_COV = $(EXECUTOR_SRCS:$(EXECUTOR_DIR)/%.c=$(BUILD_EXECUTOR_DIR)/%.cov.o)
EXECUTOR_OBJS_PIC = $(EXECUTOR_SRCS:$(EXECUTOR_DIR)/%.c=$(BUILD_EXECUTOR_DIR)/%.pic.o)

# Test sources
TEST_SRCS = \
	$(TEST_DIR)/test_runner.c \
	$(TEST_DIR)/test_parser_keywords.c \
	$(TEST_DIR)/test_scanner.c \
	$(TEST_DIR)/test_parser.c \
	$(TEST_DIR)/test_transform_create.c \
	$(TEST_DIR)/test_transform_set.c \
	$(TEST_DIR)/test_transform_delete.c \
	$(TEST_DIR)/test_transform_functions.c \
	$(TEST_DIR)/test_transform_match.c \
	$(TEST_DIR)/test_transform_return.c \
	$(TEST_DIR)/test_agtype.c \
	$(TEST_DIR)/test_schema.c \
	$(TEST_DIR)/test_executor_basic.c \
	$(TEST_DIR)/test_executor_relationships.c \
	$(TEST_DIR)/test_executor_set.c \
	$(TEST_DIR)/test_executor_delete.c \
	$(TEST_DIR)/test_executor_varlen.c \
	$(TEST_DIR)/test_executor_with.c \
	$(TEST_DIR)/test_executor_unwind.c \
	$(TEST_DIR)/test_executor_merge.c \
	$(TEST_DIR)/test_executor_pagerank.c \
	$(TEST_DIR)/test_executor_label_propagation.c \
	$(TEST_DIR)/test_executor_dijkstra.c \
	$(TEST_DIR)/test_executor_degree_centrality.c \
	$(TEST_DIR)/test_executor_components.c \
	$(TEST_DIR)/test_executor_betweenness.c \
	$(TEST_DIR)/test_executor_closeness.c \
	$(TEST_DIR)/test_executor_louvain.c \
	$(TEST_DIR)/test_executor_triangle.c \
	$(TEST_DIR)/test_executor_astar.c \
	$(TEST_DIR)/test_executor_traversal.c \
	$(TEST_DIR)/test_executor_similarity.c \
	$(TEST_DIR)/test_executor_knn.c \
	$(TEST_DIR)/test_executor_eigenvector.c \
	$(TEST_DIR)/test_executor_apsp.c \
	$(TEST_DIR)/test_executor_remove.c \
	$(TEST_DIR)/test_executor_params.c \
	$(TEST_DIR)/test_output_format.c \
	$(TEST_DIR)/test_executor_expressions.c \
	$(TEST_DIR)/test_executor_clauses.c \
	$(TEST_DIR)/test_executor_patterns.c \
	$(TEST_DIR)/test_executor_functions.c \
	$(TEST_DIR)/test_executor_predicates.c

TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_TEST_DIR)/%.o)

# Test executable
TEST_RUNNER = $(BUILD_DIR)/test_runner

# Main application executable
MAIN_APP = $(BUILD_DIR)/gqlite
MAIN_OBJ = $(BUILD_DIR)/main.o

# SQLite extension - use .dylib on macOS, .dll on Windows, .so on Linux
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    EXTENSION_LIB = $(BUILD_DIR)/graphqlite.dylib
else ifneq (,$(findstring MINGW,$(UNAME_S)))
    EXTENSION_LIB = $(BUILD_DIR)/graphqlite.dll
else ifneq (,$(findstring MSYS,$(UNAME_S)))
    EXTENSION_LIB = $(BUILD_DIR)/graphqlite.dll
else
    EXTENSION_LIB = $(BUILD_DIR)/graphqlite.so
endif
EXTENSION_OBJ = $(BUILD_DIR)/extension.o


# Default target
all: dirs $(PARSER_OBJS)

# Build main application
graphqlite: $(MAIN_APP)

# Build SQLite extension
extension: $(EXTENSION_LIB)


$(MAIN_APP): $(MAIN_OBJ) $(PARSER_OBJS) $(TRANSFORM_OBJS) $(EXECUTOR_OBJS) | dirs
	$(CC) $(CFLAGS) $^ -o $@ -lsqlite3

# SQLite extension shared library (with full parser, transform, and executor)
$(EXTENSION_LIB): $(EXTENSION_OBJ) $(PARSER_OBJS_PIC) $(TRANSFORM_OBJS_PIC) $(EXECUTOR_OBJS_PIC) | dirs $(GRAMMAR_HDR)
ifeq ($(UNAME_S),Darwin)
	$(CC) -g -fPIC -dynamiclib $(EXTENSION_OBJ) $(PARSER_OBJS_PIC) $(TRANSFORM_OBJS_PIC) $(EXECUTOR_OBJS_PIC) -o $@ -undefined dynamic_lookup
else ifneq (,$(findstring MINGW,$(UNAME_S)))
	$(CC) -shared $(EXTENSION_OBJ) $(PARSER_OBJS_PIC) $(TRANSFORM_OBJS_PIC) $(EXECUTOR_OBJS_PIC) -o $@ -lsqlite3 -lsystre
else ifneq (,$(findstring MSYS,$(UNAME_S)))
	$(CC) -shared $(EXTENSION_OBJ) $(PARSER_OBJS_PIC) $(TRANSFORM_OBJS_PIC) $(EXECUTOR_OBJS_PIC) -o $@ -lsqlite3 -lsystre
else
	$(CC) -shared -fPIC $(EXTENSION_OBJ) $(PARSER_OBJS_PIC) $(TRANSFORM_OBJS_PIC) $(EXECUTOR_OBJS_PIC) -o $@
endif

# Main application object
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Extension object (uses vendored SQLite headers for ABI consistency - no EXTRA_INCLUDES)
$(BUILD_DIR)/extension.o: $(SRC_DIR)/extension.c | dirs
	$(CC) $(EXTENSION_CFLAGS_BASE) $(EXTENSION_CFLAGS) -fPIC -c $< -o $@


# Help target
help:
	@echo "GraphQLite Makefile Commands:"
	@echo "  make           - Build parser objects (default)"
	@echo "  make all       - Same as 'make'"
	@echo "  make graphqlite - Build main interactive application"
	@echo "  make extension - Build SQLite extension (graphqlite.dylib on macOS, graphqlite.so on Linux)"
	@echo "  make test      - Run all tests (unit + functional + bindings)"
	@echo "  make test unit - Run only CUnit tests"
	@echo "  make test rust - Run Rust binding tests"
	@echo "  make test python - Run Python binding tests"
	@echo "  make test bindings - Run all binding tests (Rust + Python)"
	@echo "  make test functional - Run functional SQL tests"
	@echo "  make test-constraints - Run constraint tests (expected to fail)"
	@echo "  make performance - Run all performance tests with summary table"
	@echo "  make coverage  - Run tests and generate gcov coverage report"
	@echo "  make clean     - Remove all build artifacts"
	@echo "  make help      - Show this help message"
	@echo ""
	@echo "Build Directories:"
	@echo "  $(BUILD_DIR)/       - Main build directory"
	@echo "  $(BUILD_PARSER_DIR)/ - Parser objects"
	@echo "  $(BUILD_TEST_DIR)/   - Test objects"
	@echo "  $(COVERAGE_DIR)/     - Coverage reports"

# Create build directories
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_PARSER_DIR)
	@mkdir -p $(BUILD_TRANSFORM_DIR)
	@mkdir -p $(BUILD_EXECUTOR_DIR)
	@mkdir -p $(BUILD_TEST_DIR)
	@mkdir -p $(COVERAGE_DIR)

# Parser objects (regular build) - need build dir for generated headers
$(BUILD_PARSER_DIR)/%.o: $(PARSER_DIR)/%.c $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) -I$(BUILD_PARSER_DIR) -c $< -o $@

# Parser objects (coverage build) - need build dir for generated headers
$(BUILD_PARSER_DIR)/%.cov.o: $(PARSER_DIR)/%.c $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -I$(BUILD_PARSER_DIR) -c $< -o $@

# Generate scanner from Flex specification
$(SCANNER_SRC): $(SCANNER_L) | dirs
	$(FLEX) -o $@ $<

# Generate parser from Bison grammar
$(GRAMMAR_SRC) $(GRAMMAR_HDR): $(GRAMMAR_Y) | dirs
	$(BISON) -d -o $(GRAMMAR_SRC) $<

# Scanner objects (regular build)
$(BUILD_PARSER_DIR)/cypher_scanner.o: $(SCANNER_SRC) | dirs
	$(CC) $(CFLAGS) -Wno-sign-compare -c $< -o $@

# Scanner objects (coverage build)
$(BUILD_PARSER_DIR)/cypher_scanner.cov.o: $(SCANNER_SRC) | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -Wno-sign-compare -c $< -o $@

# Grammar objects (regular build)
$(BUILD_PARSER_DIR)/cypher_gram.tab.o: $(GRAMMAR_SRC) $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) -Wno-unused-but-set-variable -I$(BUILD_PARSER_DIR) -c $< -o $@

# Grammar objects (coverage build)
$(BUILD_PARSER_DIR)/cypher_gram.tab.cov.o: $(GRAMMAR_SRC) $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -Wno-unused-but-set-variable -I$(BUILD_PARSER_DIR) -c $< -o $@

# Transform objects
$(BUILD_TRANSFORM_DIR)/%.o: $(TRANSFORM_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Transform objects (coverage build)
$(BUILD_TRANSFORM_DIR)/%.cov.o: $(TRANSFORM_DIR)/%.c | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

# Executor objects
$(BUILD_EXECUTOR_DIR)/%.o: $(EXECUTOR_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Executor objects (coverage build)
$(BUILD_EXECUTOR_DIR)/%.cov.o: $(EXECUTOR_DIR)/%.c | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

# PIC object builds for shared library (uses vendored SQLite headers only - no EXTRA_INCLUDES)
$(BUILD_PARSER_DIR)/%.pic.o: $(PARSER_DIR)/%.c $(GRAMMAR_HDR) | dirs
	$(CC) $(EXTENSION_CFLAGS_BASE) $(EXTENSION_CFLAGS) -fPIC -I$(BUILD_PARSER_DIR) -c $< -o $@

$(BUILD_PARSER_DIR)/cypher_scanner.pic.o: $(SCANNER_SRC) | dirs
	$(CC) $(EXTENSION_CFLAGS_BASE) $(EXTENSION_CFLAGS) -fPIC -Wno-sign-compare -c $< -o $@

$(BUILD_PARSER_DIR)/cypher_gram.tab.pic.o: $(GRAMMAR_SRC) $(GRAMMAR_HDR) | dirs
	$(CC) $(EXTENSION_CFLAGS_BASE) $(EXTENSION_CFLAGS) -fPIC -Wno-unused-but-set-variable -I$(BUILD_PARSER_DIR) -c $< -o $@

$(BUILD_TRANSFORM_DIR)/%.pic.o: $(TRANSFORM_DIR)/%.c | dirs
	$(CC) $(EXTENSION_CFLAGS_BASE) $(EXTENSION_CFLAGS) -fPIC -c $< -o $@

$(BUILD_EXECUTOR_DIR)/%.pic.o: $(EXECUTOR_DIR)/%.c | dirs
	$(CC) $(EXTENSION_CFLAGS_BASE) $(EXTENSION_CFLAGS) -fPIC -c $< -o $@

# Test objects
$(BUILD_TEST_DIR)/%.o: $(TEST_DIR)/%.c $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) -I$(BUILD_PARSER_DIR) -c $< -o $@

# Test runner executable
$(TEST_RUNNER): $(TEST_OBJS) $(PARSER_OBJS_COV) $(TRANSFORM_OBJS_COV) $(EXECUTOR_OBJS_COV) | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $^ -o $@ $(LDFLAGS) $(COVERAGE_LIBS)

# Run constraint tests (expected to fail with specific errors)
test-constraints: extension
	@echo "Running constraint tests (expected to fail)..."
	@for test_file in tests/functional/*constraint*.sql; do \
		if [ -f "$$test_file" ]; then \
			echo ""; \
			echo "========================================"; \
			echo "Running: $$(basename $$test_file)"; \
			echo "========================================"; \
			$(SQLITE) < "$$test_file" 2>&1 && echo "ERROR: Test should have failed!" || echo "Constraint correctly enforced"; \
		fi; \
	done
	@echo ""
	@echo "All functional tests completed successfully!"

# Generate coverage report
coverage: test
	@echo "Generating coverage report..."
	@for obj in $(BUILD_PARSER_DIR)/*.cov.o; do \
		gcov -o $(BUILD_PARSER_DIR) $$obj; \
	done
	@for obj in $(BUILD_TRANSFORM_DIR)/*.cov.o; do \
		gcov -o $(BUILD_TRANSFORM_DIR) $$obj; \
	done
	@for obj in $(BUILD_EXECUTOR_DIR)/*.cov.o; do \
		gcov -o $(BUILD_EXECUTOR_DIR) $$obj; \
	done
	@find . -name "*.gcov" -maxdepth 1 -exec mv {} $(COVERAGE_DIR)/ \;
	@echo ""
	@echo "========== CODE COVERAGE SUMMARY =========="
	@for file in $(COVERAGE_DIR)/*.gcov; do \
		if [ -f "$$file" ]; then \
			filename=$$(basename $$file .gcov); \
			coverage=$$(grep -E "^[[:space:]]*[0-9]+:" $$file | wc -l); \
			total=$$(grep -E "^[[:space:]]*[0-9]+:|[[:space:]]*#####:" $$file | wc -l); \
			if [ $$total -gt 0 ]; then \
				percent=$$(echo "scale=2; $$coverage * 100 / $$total" | bc); \
				printf "%-40s %6.2f%%\n" "$$filename:" "$$percent"; \
			fi; \
		fi; \
	done
	@echo "==========================================="
	@echo ""
	@echo "Detailed reports in: $(COVERAGE_DIR)/"

# Performance tests - unified script with modes: quick, standard, full
# Usage: make performance [MODE=quick|standard|full] [ITERATIONS=N]
performance: extension
	@tests/performance/run_all_perf.sh $(or $(MODE),standard) 2>&1 | grep -v "^\[CYPHER_DEBUG\]"

# Quick performance check (~30s, 10K nodes only)
performance-quick: extension
	@tests/performance/run_all_perf.sh quick 2>&1 | grep -v "^\[CYPHER_DEBUG\]"

# Full performance suite (~10min, up to 1M nodes)
performance-full: extension
	@tests/performance/run_all_perf.sh full 2>&1 | grep -v "^\[CYPHER_DEBUG\]"

# Bindings directories
RUST_BINDINGS_DIR = bindings/rust
PYTHON_BINDINGS_DIR = bindings/python

# Nested test commands: make test [unit|rust|python|bindings|functional]
# Check what subcommand was passed
ifneq ($(filter unit,$(MAKECMDGOALS)),)
TEST_TARGET = unit
else ifneq ($(filter rust,$(MAKECMDGOALS)),)
TEST_TARGET = rust
else ifneq ($(filter python,$(MAKECMDGOALS)),)
TEST_TARGET = python
else ifneq ($(filter bindings,$(MAKECMDGOALS)),)
TEST_TARGET = bindings
else ifneq ($(filter functional,$(MAKECMDGOALS)),)
TEST_TARGET = functional
else
TEST_TARGET = all
endif

# Dummy targets for subcommands (prevents "No rule to make target" errors)
unit rust python bindings functional:
	@true

# Individual test targets
test-unit: $(TEST_RUNNER)
	@echo "Running unit tests..."
	./$(TEST_RUNNER)

test-rust: extension
	@echo "Running Rust binding tests..."
	cd $(RUST_BINDINGS_DIR) && cargo test -- --test-threads=1

test-python: extension
	@echo "Running Python binding tests..."
	@# Install in dev mode if not already installed, then run tests
	@# Use $(PYTHON) -m pip to ensure pip matches the Python interpreter
	@cd $(PYTHON_BINDINGS_DIR) && $(PYTHON) -m pip install -q -e . 2>/dev/null || true
	cd $(PYTHON_BINDINGS_DIR) && $(PYTHON) -m pytest tests/ -v

test-bindings: test-rust test-python

test-functional: extension
	@echo "Running functional tests..."
	@for test_file in tests/functional/*.sql; do \
		if [ -f "$$test_file" ] && [[ "$$test_file" != *"constraint"* ]]; then \
			echo "Running: $$(basename $$test_file)"; \
			$(SQLITE) -bail < "$$test_file" || exit 1; \
		fi; \
	done

test-all: test-unit test-functional test-bindings

# Main test target dispatches to appropriate sub-target
test: test-$(TEST_TARGET)

# Clean
clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete
	find . -name "*.gcov" -delete

.PHONY: all help dirs test test-unit test-rust test-python test-bindings test-functional test-all test-constraints test-perf test-perf-quick test-perf-scaled test-perf-pagerank performance coverage clean unit rust python bindings functional