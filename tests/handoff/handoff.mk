# THANDOFF.8xp, the app side of the handoff test. Run from the repo root
# (`make handoff`) so no source path needs `..`: CEdev on Windows can't
# build those.

NAME = THANDOFF
DESCRIPTION = "TINCLIBC handoff test"
ICON =
COMPRESSED = NO

SRCDIR = tests/handoff
OBJDIR = obj/handoff
BINDIR = bin/handoff

CFLAGS = -Wall -Wextra -Oz -Ilib/tinclib/src -Ilib/tinclib-protocol
EXTRA_C_SOURCES = $(wildcard lib/tinclib/src/*.c) \
	lib/tinclib-protocol/crc16.c lib/tinclib-protocol/tinc_frame.c

include $(shell cedev-config --makefile)
