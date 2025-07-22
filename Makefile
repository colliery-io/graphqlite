# GraphQLite OpenCypher Implementation
# BISON + FLEX based parser

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -fPIC
LIBS = -lsqlite3

# Debug build support
ifdef DEBUG
CFLAGS += -DDEBUG -O0
endif

# Directories
SRC_DIR = src
GRAMMAR_DIR = grammar
BUILD_DIR = build
TEST_DIR = tests

# BISON and FLEX
BISON = bison
FLEX = flex
GRAMMAR_FILE = $(GRAMMAR_DIR)/cypher.y
LEXER_FILE = $(GRAMMAR_DIR)/cypher.l
PARSER_C = $(GRAMMAR_DIR)/cypher.tab.c
PARSER_H = $(GRAMMAR_DIR)/cypher.tab.h
LEXER_C = $(GRAMMAR_DIR)/lex.yy.c

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Generated parser objects
PARSER_OBJ = $(BUILD_DIR)/cypher.tab.o
LEXER_OBJ = $(BUILD_DIR)/lex.yy.o

# Test files
TEST_RUNNER = $(TEST_DIR)/test_runner.c
TEST_SOURCES = $(filter-out $(TEST_RUNNER),$(wildcard $(TEST_DIR)/test_*.c))
TEST_TARGET = $(BUILD_DIR)/test_runner

# Target library and SQLite extension
TARGET = $(BUILD_DIR)/libgraphqlite.a
EXTENSION = $(BUILD_DIR)/graphqlite.so

.PHONY: all clean test test-unit test-sql parser extension

all: parser $(TARGET) $(EXTENSION)

# Generate parser files
parser: $(PARSER_C) $(LEXER_C)

$(PARSER_C) $(PARSER_H): $(GRAMMAR_FILE)
	cd $(GRAMMAR_DIR) && $(BISON) -d cypher.y

$(LEXER_C): $(LEXER_FILE) $(PARSER_H)
	cd $(GRAMMAR_DIR) && $(FLEX) cypher.l

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(GRAMMAR_DIR) -c $< -o $@

# Compile generated parser
$(PARSER_OBJ): $(PARSER_C) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(PARSER_C) -o $@

# Compile generated lexer
$(LEXER_OBJ): $(LEXER_C) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(LEXER_C) -o $@

# Build static library
$(TARGET): $(OBJECTS) $(PARSER_OBJ) $(LEXER_OBJ) | $(BUILD_DIR)
	ar rcs $@ $^

# Build SQLite extension (shared library)
# Note: SQLite extensions must not link against libsqlite3
# Use dynamic symbol resolution instead
$(EXTENSION): $(OBJECTS) $(PARSER_OBJ) $(LEXER_OBJ) | $(BUILD_DIR)
ifeq ($(shell uname -s),Darwin)
	$(CC) -shared -fPIC -o $@ $^ -undefined dynamic_lookup
else
	$(CC) -shared -fPIC -o $@ $^
endif

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Test suite with parser and SQL tests
test: $(TEST_TARGET) $(EXTENSION)
	$(TEST_TARGET)
	./tests/run_sql_tests.sh

# Run only unit tests
test-unit: $(TEST_TARGET)
	$(TEST_TARGET)

# Run only SQL tests
test-sql: $(EXTENSION)
	./tests/run_sql_tests.sh

$(TEST_TARGET): $(TEST_SOURCES) $(TEST_RUNNER) $(OBJECTS) $(PARSER_OBJ) $(LEXER_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) `pkg-config --cflags cunit` -I$(SRC_DIR) -I$(GRAMMAR_DIR) $(TEST_SOURCES) $(TEST_RUNNER) $(OBJECTS) $(PARSER_OBJ) $(LEXER_OBJ) -o $@ `pkg-config --libs cunit` $(LIBS)

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(PARSER_C) $(PARSER_H) $(LEXER_C)
	@echo "Cleaned build directory and generated files"

info:
	@echo "GraphQLite OpenCypher - BISON/FLEX Implementation"
	@echo "Parser files: $(PARSER_C) $(PARSER_H) $(LEXER_C)"
	@echo "Source files: $(SOURCES)"
	@echo "Target: $(TARGET)"
	@echo "Available targets: all, clean, test, parser, info"