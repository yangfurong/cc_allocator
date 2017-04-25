CC=gcc
CFLAGS=-O3
LDFLAGS=

all: cc_allocator.c
	$(CC) $^ $(CFLAGS) -o $@ $(LDFLAGS)
