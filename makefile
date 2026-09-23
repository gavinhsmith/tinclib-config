# ----------------------------
# Makefile Options
# ----------------------------

NAME = TINCLIBC
ICON = icon.png
DESCRIPTION = "TINCLIB config"
COMPRESSED = NO

# Dependencies are git submodules under lib/ (CEdev on Windows can't build
# sources reached through `..`). tinclib-protocol is header-only here until
# there is a link to use its framing code.
CFLAGS = -Wall -Wextra -Oz -Ilib/titrmlib/src -Ilib/tinclib-protocol
CXXFLAGS = -Wall -Wextra -Oz -Ilib/titrmlib/src -Ilib/tinclib-protocol
EXTRA_C_SOURCES = $(wildcard lib/titrmlib/src/*.c)

# ----------------------------

include $(shell cedev-config --makefile)

.PHONY: host-test relaunch
host-test:
	$(MAKE) -C tests

# os_RunPrgm chaining spike: see tests/relaunch/.
relaunch:
	python -c "import shutil; shutil.copy('src/handoff.c', 'tests/relaunch/src/handoff.c')"
	$(MAKE) -C tests/relaunch
	$(MAKE) -C tests/relaunch fixture
