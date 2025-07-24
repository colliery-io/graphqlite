# GraphQLite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -g -I./src/include -I/opt/local/include
LDFLAGS = -L/opt/local/lib -lcunit

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
BUILD_TEST_DIR = $(BUILD_DIR)/tests
COVERAGE_DIR = $(BUILD_DIR)/coverage

# Parser objects
PARSER_SRCS = \
	$(PARSER_DIR)/cypher_keywords.c

PARSER_OBJS = $(PARSER_SRCS:$(PARSER_DIR)/%.c=$(BUILD_PARSER_DIR)/%.o)
PARSER_OBJS_COV = $(PARSER_SRCS:$(PARSER_DIR)/%.c=$(BUILD_PARSER_DIR)/%.cov.o)

# Test sources
TEST_SRCS = \
	$(TEST_DIR)/test_runner.c \
	$(TEST_DIR)/test_parser_keywords.c

TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_TEST_DIR)/%.o)

# Test executable
TEST_RUNNER = $(BUILD_DIR)/test_runner

# Default target
all: dirs $(PARSER_OBJS)

# Help target
help:
	@echo "GraphQLite Makefile Commands:"
	@echo "  make         - Build parser objects (default)"
	@echo "  make all     - Same as 'make'"
	@echo "  make test    - Build and run CUnit tests"
	@echo "  make coverage - Run tests and generate gcov coverage report"
	@echo "  make clean   - Remove all build artifacts"
	@echo "  make help    - Show this help message"
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
	@mkdir -p $(BUILD_TEST_DIR)
	@mkdir -p $(COVERAGE_DIR)

# Parser objects (regular build)
$(BUILD_PARSER_DIR)/%.o: $(PARSER_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Parser objects (coverage build)
$(BUILD_PARSER_DIR)/%.cov.o: $(PARSER_DIR)/%.c | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

# Test objects
$(BUILD_TEST_DIR)/%.o: $(TEST_DIR)/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

# Test runner executable
$(TEST_RUNNER): $(TEST_OBJS) $(PARSER_OBJS_COV) | dirs
	$(CC) $(CFLAGS) $(COVERAGE_FLAGS) $^ -o $@ $(LDFLAGS) $(COVERAGE_LIBS)

# Run tests
test: $(TEST_RUNNER)
	./$(TEST_RUNNER)

# Generate coverage report
coverage: test
	@echo "Generating coverage report..."
	@for obj in $(BUILD_PARSER_DIR)/*.cov.o; do \
		gcov -o $(BUILD_PARSER_DIR) $$obj; \
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