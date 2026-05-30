CC      = gcc
CFLAGS  = -Wall -Wextra -g -I./include
BUILD_DIR = build
TARGET  = $(BUILD_DIR)/lb
SRCS    = $(wildcard src/*.c)
OBJS    = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

run:
	./$(TARGET)
