# GraphQLite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -I./src/include -I/opt/local/include
LDFLAGS = -L/opt/local/lib -lcunit -lsqlite3

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
	$(TRANSFORM_DIR)/transform_return.c

# Executor sources
EXECUTOR_DIR = $(SRC_DIR)/backend/executor
EXECUTOR_SRCS = \
	$(EXECUTOR_DIR)/cypher_schema.c \
	$(EXECUTOR_DIR)/cypher_executor.c \
	$(EXECUTOR_DIR)/agtype.c

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
	$(TEST_DIR)/test_transform.c \
	$(TEST_DIR)/test_schema.c \
	$(TEST_DIR)/test_executor.c

TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_TEST_DIR)/%.o)

# Test executable
TEST_RUNNER = $(BUILD_DIR)/test_runner

# Main application executable
MAIN_APP = $(BUILD_DIR)/graphqlite
MAIN_OBJ = $(BUILD_DIR)/main.o

# SQLite extension - use .dylib on macOS, .so on Linux
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    EXTENSION_LIB = $(BUILD_DIR)/graphqlite.dylib
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
else
	$(CC) -shared -fPIC $(EXTENSION_OBJ) $(PARSER_OBJS_PIC) $(TRANSFORM_OBJS_PIC) $(EXECUTOR_OBJS_PIC) -o $@
endif

# Main application object
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Extension object
$(BUILD_DIR)/extension.o: $(SRC_DIR)/extension.c | dirs
	$(CC) $(CFLAGS) -fPIC -I$(SRC_DIR) -c $< -o $@


# Help target
help:
	@echo "GraphQLite Makefile Commands:"
	@echo "  make           - Build parser objects (default)"
	@echo "  make all       - Same as 'make'"
	@echo "  make graphqlite - Build main interactive application"
	@echo "  make extension - Build SQLite extension (graphqlite.dylib on macOS, graphqlite.so on Linux)"
	@echo "  make test      - Build and run CUnit tests"
	@echo "  make test-functional - Build extension and run functional SQL tests"
	@echo "  make test-constraints - Run constraint tests (expected to fail)"
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
	flex -o $@ $<

# Generate parser from Bison grammar
$(GRAMMAR_SRC) $(GRAMMAR_HDR): $(GRAMMAR_Y) | dirs
	bison -d -o $(GRAMMAR_SRC) $<

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

# PIC object builds for shared library
$(BUILD_PARSER_DIR)/%.pic.o: $(PARSER_DIR)/%.c $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) -fPIC -I$(BUILD_PARSER_DIR) -c $< -o $@

$(BUILD_PARSER_DIR)/cypher_scanner.pic.o: $(SCANNER_SRC) | dirs
	$(CC) $(CFLAGS) -fPIC -Wno-sign-compare -c $< -o $@

$(BUILD_PARSER_DIR)/cypher_gram.tab.pic.o: $(GRAMMAR_SRC) $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) -fPIC -Wno-unused-but-set-variable -I$(BUILD_PARSER_DIR) -c $< -o $@

$(BUILD_TRANSFORM_DIR)/%.pic.o: $(TRANSFORM_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD_EXECUTOR_DIR)/%.pic.o: $(EXECUTOR_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

# Test objects
$(BUILD_TEST_DIR)/%.o: $(TEST_DIR)/%.c $(GRAMMAR_HDR) | dirs
	$(CC) $(CFLAGS) -I$(BUILD_PARSER_DIR) -c $< -o $@

# Test runner executable
$(TEST_RUNNER): $(TEST_OBJS) $(PARSER_OBJS_COV) $(TRANSFORM_OBJS_COV) $(EXECUTOR_OBJS_COV) | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $^ -o $@ $(LDFLAGS) $(COVERAGE_LIBS)

# Run tests
test: $(TEST_RUNNER)
	./$(TEST_RUNNER)

# Run functional tests with SQLite extension (excludes constraint error tests)
test-functional: extension
	@echo "Running functional tests..."
	@for test_file in tests/functional/*.sql; do \
		if [ -f "$$test_file" ] && [[ "$$test_file" != *"constraint"* ]]; then \
			echo ""; \
			echo "========================================"; \
			echo "Running: $$(basename $$test_file)"; \
			echo "========================================"; \
			sqlite3 -bail < "$$test_file" || exit 1; \
		fi; \
	done
	@echo ""
	@echo "All functional tests completed successfully!"

# Run constraint tests (expected to fail with specific errors)
test-constraints: extension
	@echo "Running constraint tests (expected to fail)..."
	@for test_file in tests/functional/*constraint*.sql; do \
		if [ -f "$$test_file" ]; then \
			echo ""; \
			echo "========================================"; \
			echo "Running: $$(basename $$test_file)"; \
			echo "========================================"; \
			sqlite3 < "$$test_file" 2>&1 && echo "ERROR: Test should have failed!" || echo "Constraint correctly enforced"; \
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

# Clean
clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete
	find . -name "*.gcov" -delete

.PHONY: all help dirs test coverage clean