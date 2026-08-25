CC = gcc
CSTD = -std=c99
DEFINES = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
CFLAGS = -Wall -Wextra $(CSTD) $(DEFINES)

SRC = src/main.c src/lexer.c src/parser.c src/hop.c src/reveal.c src/peek.c src/locate.c src/executor.c
OBJ = $(SRC:.c=.o)
EXEC = shell.out

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(EXEC)