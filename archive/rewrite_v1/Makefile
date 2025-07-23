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
CYPHER_DIR = src/cypher
TOOLS_DIR = tools
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
CYPHER_OBJ_DIR = $(BUILD_DIR)/cypher_obj

# BISON parser generator
BISON = bison
GRAMMAR_FILE = $(TOOLS_DIR)/cypher.y
PARSER_C = $(TOOLS_DIR)/cypher.tab.c
PARSER_H = $(TOOLS_DIR)/cypher.tab.h

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
CYPHER_SOURCES = $(wildcard $(CYPHER_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CYPHER_OBJECTS = $(CYPHER_SOURCES:$(CYPHER_DIR)/%.c=$(CYPHER_OBJ_DIR)/%.o)

# Include generated parser
PARSER_OBJ = $(BUILD_DIR)/cypher.o

# Target library
TARGET = $(BUILD_DIR)/libgraphqlite.so

# Test files
TEST_DIR = tests
TEST_RUNNER = $(TEST_DIR)/test_runner.c
TEST_SOURCES = $(filter-out $(TEST_RUNNER),$(wildcard $(TEST_DIR)/test_*.c))
TEST_TARGET = $(BUILD_DIR)/test_runner

# Header files
HEADERS = $(SRC_DIR)/graphqlite_internal.h
CYPHER_HEADERS = $(CYPHER_DIR)/cypher_lexer.h $(CYPHER_DIR)/cypher_ast.h $(CYPHER_DIR)/cypher_parser.h $(CYPHER_DIR)/cypher_executor.h

.PHONY: all clean test install

all: $(TARGET)

$(TARGET): $(OBJECTS) $(CYPHER_OBJECTS) $(PARSER_OBJ) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(CYPHER_OBJ_DIR)/%.o: $(CYPHER_DIR)/%.c $(CYPHER_HEADERS) | $(CYPHER_OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Note: cypher_parser.c no longer exists - BISON generates parser directly

# BISON parser generation rules  
$(PARSER_C) $(PARSER_H): $(GRAMMAR_FILE)
	cd $(TOOLS_DIR) && $(BISON) -d cypher.y
	@test -f $(PARSER_C) && test -f $(PARSER_H) || (echo "BISON failed to generate parser files" && exit 1)

$(PARSER_OBJ): $(PARSER_C) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(PARSER_C) -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(CYPHER_OBJ_DIR):
	mkdir -p $(CYPHER_OBJ_DIR)

clean:
	rm -rf $(BUILD_DIR)
	# Clean up generated parser files
	rm -f $(PARSER_C) $(PARSER_H)
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

$(TEST_TARGET): $(TEST_SOURCES) $(TEST_RUNNER) $(OBJECTS) $(CYPHER_OBJECTS) $(PARSER_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) `pkg-config --cflags cunit` -I$(SRC_DIR) -I$(CYPHER_DIR) $(TEST_SOURCES) $(TEST_RUNNER) $(OBJECTS) $(CYPHER_OBJECTS) $(PARSER_OBJ) -o $@ `pkg-config --libs cunit` $(LIBS)

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