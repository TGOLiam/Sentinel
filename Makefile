CC      = gcc
CFLAGS  = -Wall -Wextra -g -I./include
TARGET  = lb
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
