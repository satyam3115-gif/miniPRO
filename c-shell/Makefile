CC = gcc
CSTD = -std=c23
DEFINES = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
CFLAGS = -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm $(CSTD) $(DEFINES)

SRC = src/main.c src/lexer.c src/parser.c src/hop.c src/reveal.c src/peek.c src/locate.c src/executor.c src/jobs.c src/resume.c src/ping_cmd.c src/spy.c src/snoop.c
OBJ = $(SRC:.c=.o)
EXEC = shell.out

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(EXEC)