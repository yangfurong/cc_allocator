CC=gcc
CFLAGS=-O3
LDFLAGS=
APP=all
TESTER=miss

$(APP): cc_allocator.c
	$(CC) $^ $(CFLAGS) -o $@ $(LDFLAGS)

$(TESTER): cc_allocator.c test_miss.c
	$(CC) $^ $(CFLAGS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(APP) $(TESTER)
