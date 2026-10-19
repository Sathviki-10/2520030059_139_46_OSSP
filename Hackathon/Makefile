
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -pthread

SRC = src/main.c src/philosophers.c
OBJ = $(SRC:.c=.o)
TARGET = dining_os_lab

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

src/%.o: src/%.c src/oslab.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

test: $(TARGET)
	bash tests/run_tests.sh

clean:
	rm -f $(OBJ) $(TARGET) dining_log.txt

