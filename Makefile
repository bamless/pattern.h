CC      ?= $(CC)
CFLAGS  ?= -Wall -Wextra -std=c99 -ggdb
LDFLAGS ?=

.PHONY: test
test: test/test
	./test/test

test/test: ./test/test.c ./test/ctest.h pattern.h
	$(CC) $(CFLAGS) -Wno-attributes -Wno-pragmas -std=c99 $(LDFLAGS) -I./test/ ./test/test.c -o test/test
