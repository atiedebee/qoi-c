CSRC = $(wildcard *.c)
COBJ = $(addprefix bin/, $(CSRC:.c=.o))

CC = gcc
CFLAGS = -O3 -Wall -Wextra


default: $(COBJ)
	$(CC) $(COBJ) $(CFLAGS)



bin/%.o:%.c
	$(CC) $(CFLAGS) $^ -c -o $@

