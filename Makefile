CC      ?= gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O1 -Icommon
LDLIBS  = -lm
COMMON  = common/can_protocol.c common/ntc.c common/mcp2515.c

.PHONY: test clean
test: build/test_protocol build/test_ntc build/test_mcp2515
	@set -e; for t in $^; do ./$$t; done

build/test_%: tests/test_%.c $(COMMON) $(wildcard common/*.h tests/test.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests $< $(COMMON) -o $@ $(LDLIBS)

clean:
	rm -rf build
