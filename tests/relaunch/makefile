# Relaunch spike: TSTRET.8xp plus a TINCHND.8xv fixture asking TINCLIBC to
# return to TSTRET. Build from the repo root with `make relaunch`.
#
# `make relaunch` copies ../../src/handoff.c into src/ first (gitignored):
# CEdev on Windows can't build sources reached through `..`.

NAME = TSTRET
DESCRIPTION = "TINCLIBC relaunch test"
COMPRESSED = NO

CFLAGS = -Wall -Wextra -Oz -I../../src -I../../lib/tinclib-protocol

include $(shell cedev-config --makefile)

fixture: bin/TINCHND.8xv

bin/TINCHND.8xv: make_fixture.py
	python make_fixture.py bin/TINCHND.bin
	convbin -j bin -k 8xv -n TINCHND -i bin/TINCHND.bin -o $@
