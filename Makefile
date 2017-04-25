CC=gcc
CFLAGS=-O3
LDFLAGS=
APP=all

$(APP): cc_allocator.c
	$(CC) $^ $(CFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(APP)
