# Host-side unit tests for the platform-free code in src/.
#
#     make -C tests           # build and run
#     make -C tests SANITIZE= # without ASan/UBSan

CC ?= cc
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
CFLAGS ?= -std=c99 -g -O1 -Wall -Wextra -Werror
CPPFLAGS += -I../src -I../lib/tinclib-protocol

BUILD := build
TEST := $(BUILD)/test_handoff

.PHONY: all test clean

all: test

test: $(TEST)
	./$(TEST)

$(TEST): test_handoff.c ../src/handoff.c ../src/handoff.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZE) -o $@ test_handoff.c ../src/handoff.c

clean:
	rm -rf $(BUILD)
