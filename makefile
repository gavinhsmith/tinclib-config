# ----------------------------
# Makefile Options
# ----------------------------

NAME = TINCLIBC
ICON = icon.png
DESCRIPTION = "TINCLIB config"
COMPRESSED = NO

# Dependencies are git submodules under lib/ (CEdev on Windows can't build
# sources reached through `..`). tinclib's own nested copy of the protocol
# isn't used: CI checks it pins the same commit as lib/tinclib-protocol.
INCLUDES = -Ilib/titrmlib/src -Ilib/tinclib/src -Ilib/tinclib-protocol
CFLAGS = -Wall -Wextra -Oz $(INCLUDES)
CXXFLAGS = -Wall -Wextra -Oz $(INCLUDES)
EXTRA_C_SOURCES = $(wildcard lib/titrmlib/src/*.c) $(wildcard lib/tinclib/src/*.c) 	lib/tinclib-protocol/crc16.c lib/tinclib-protocol/tinc_frame.c

# ----------------------------

include $(shell cedev-config --makefile)

.PHONY: host-test handoff
host-test:
	$(MAKE) -C tests

# App side of the handoff emulator test: see tests/handoff/.
handoff:
	$(MAKE) -f tests/handoff/handoff.mk
