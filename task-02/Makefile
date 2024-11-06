CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -g
SRC = texDi.c
EXEC = texDi
all: $(EXEC)
$(EXEC): $(SRC)
	$(CC) $(CFLAGS) $< -o $@
clean:
	rm -f $(EXEC)
	
.PHONY: all clean

