CC = gcc
CFLAGS = -O3 -Wall -Wextra


default:
	$(CC) $(CFLAGS) qoi.c -c -o bin/qoi.o

.PHONY: shared clean test


shared: default
	$(CC) $(CFLAGS) bin/qoi.o -fPIC -shared -o qoi.so

test:
	$(CC) $(CFLAGS) qoi.c test.c
clean:
	@rm bin/qoi.o qoi.so a.out
