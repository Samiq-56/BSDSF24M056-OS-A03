CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = src/main.c src/shell.c src/execute.c
OBJ = $(SRC:src/%.c=obj/%.o)
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) $(OBJ) -o $(TARGET)

obj/%.o: src/%.c
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin

.PHONY: all clean
