CC = gcc
CFLAGS = -O3 -Wall -Wextra -fuse-ld=lld -fpic

CSRC = qoi.c

default:
	$(CC) $(CFLAGS) $(CSRC) -c -o bin/qoi.o


.PHONY: shared clean


shared: default
	$(CC) $(CFLAGS) bin/qoi.o -shared -o qoi.so

clean:
	@rm bin/qoi.o qoi.so
