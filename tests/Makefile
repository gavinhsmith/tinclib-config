# Host-side unit tests for the platform-free code in src/, plus the handoff
# interop test against tinclib's real handoff code on tinclib's host stubs.
#
#     make -C tests           # build and run
#     make -C tests SANITIZE= # without ASan/UBSan

CC ?= cc
SANITIZE ?= -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all
CFLAGS ?= -std=c99 -g -O1 -Wall -Wextra -Werror

PROTO := ../lib/tinclib-protocol
TINC := ../lib/tinclib
CPPFLAGS += -I../src -I$(PROTO)

BUILD := build
TESTS := $(BUILD)/test_handoff $(BUILD)/test_admin $(BUILD)/test_interop

.PHONY: all test clean

all: test

test: $(TESTS)
	@for t in $(TESTS); do ./$$t || exit 1; done

$(BUILD)/test_handoff: test_handoff.c ../src/handoff.c ../src/handoff.h
	@mkdir -p $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SANITIZE) -o $@ test_handoff.c ../src/handoff.c

$(BUILD)/test_admin: test_admin.c ../src/admin.c ../src/admin.h
	@mkdir -p $(BUILD)
	$(CC) -I$(TINC)/src $(CPPFLAGS) $(CFLAGS) $(SANITIZE) -o $@ test_admin.c ../src/admin.c

# tinclib's stubs rename clock() so time only moves when the library looks.
INTEROP_SRC := test_interop.c ../src/handoff.c ../src/admin.c $(TINC)/tests/stubs/stubs.c \
	$(wildcard $(TINC)/src/*.c) $(PROTO)/crc16.c $(PROTO)/tinc_frame.c
$(BUILD)/test_interop: $(INTEROP_SRC) ../src/handoff.h ../src/admin.h
	@mkdir -p $(BUILD)
	$(CC) -I$(TINC)/tests/stubs -I$(TINC)/src $(CPPFLAGS) -Dclock=tinc_fake_clock \
		$(CFLAGS) $(SANITIZE) -o $@ $(INTEROP_SRC)

clean:
	rm -rf $(BUILD)
