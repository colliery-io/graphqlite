# GraphQLite Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -fPIC -O2 -g
LDFLAGS = -shared
LIBS = -lsqlite3 -lpthread -lm

# Debug build support (similar to sqlite-vec approach)
ifdef DEBUG
CFLAGS += -DGRAPHQLITE_DEBUG
endif

# Directories
SRC_DIR = src/core
GQL_DIR = src/gql
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
GQL_OBJ_DIR = $(BUILD_DIR)/gql_obj

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
GQL_SOURCES = $(wildcard $(GQL_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
GQL_OBJECTS = $(GQL_SOURCES:$(GQL_DIR)/%.c=$(GQL_OBJ_DIR)/%.o)

# Target library
TARGET = $(BUILD_DIR)/libgraphqlite.so

# Test files
TEST_DIR = tests
TEST_RUNNER = $(TEST_DIR)/test_runner.c
TEST_SOURCES = $(filter-out $(TEST_RUNNER),$(wildcard $(TEST_DIR)/test_*.c))
TEST_TARGET = $(BUILD_DIR)/test_runner

# Header files
HEADERS = $(SRC_DIR)/graphqlite_internal.h
GQL_HEADERS = $(GQL_DIR)/gql_lexer.h $(GQL_DIR)/gql_ast.h $(GQL_DIR)/gql_parser.h $(GQL_DIR)/gql_executor.h $(GQL_DIR)/gql_matcher.h

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJECTS) $(GQL_OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(GQL_OBJ_DIR)/%.o: $(GQL_DIR)/%.c $(GQL_HEADERS) | $(GQL_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(GQL_OBJ_DIR):
	mkdir -p $(GQL_OBJ_DIR)

clean:
	rm -rf $(BUILD_DIR)
	# Clean up test binaries (portable approach)
	@for file in $(TEST_DIR)/*; do \
		if [ -f "$$file" ] && [ -x "$$file" ] && [ ! "$${file##*.}" = "c" ] && [ ! "$${file##*.}" = "h" ]; then \
			echo "Removing executable: $$file"; \
			rm -f "$$file"; \
		fi; \
	done
	# Remove .dSYM files on macOS
	rm -rf $(TEST_DIR)/*.dSYM

# CUnit test suite
test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SOURCES) $(TEST_RUNNER) $(OBJECTS) $(GQL_OBJECTS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) `pkg-config --cflags cunit` -I$(SRC_DIR) $(TEST_SOURCES) $(TEST_RUNNER) $(OBJECTS) $(GQL_OBJECTS) -o $@ `pkg-config --libs cunit` $(LIBS)

install: $(TARGET)
	cp $(TARGET) /usr/local/lib/
	cp $(SRC_DIR)/graphqlite_internal.h /usr/local/include/

# Debug build
debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

# Show build information
info:
	@echo "Source files: $(SOURCES)"
	@echo "Object files: $(OBJECTS)"
	@echo "Target: $(TARGET)"

.PHONY: all clean test install debug info