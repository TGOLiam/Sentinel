CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11 -I./include 
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TARGET  = $(BUILD_DIR)/lb
SRCS    = $(wildcard src/*.c)
OBJS    = $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: src/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(BUILD_DIR)

run:
	./$(TARGET)
