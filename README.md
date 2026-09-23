# tinclib-config

**TINCLIBC.8xp**: the TI-84 Plus CE program that owns TINCLIB board configuration
(Wi-Fi slots, the global insecure-TLS toggle) and is where apps hand off
to when they need setup. See [AGENTS.md](AGENTS.md) for the design rules.

Status: skeleton. The UI shell and the `TINCHND` handoff work. Nothing talks to
the board yet: that needs `tinclib`'s serial link, and admin messages beyond
protocol 0.1.

## Setup
Install the [CE C toolchain](https://ce-programming.github.io/toolchain/) (v15.0) and put `CEdev/bin` on your `PATH`.
Dependencies are pinned git submodules under `lib/`:

| Submodule | Tag |
|---|---|
| [tinclib-protocol](https://github.com/gavinhsmith/tinclib-protocol) | v0.1.0 |
| [titrmlib](https://github.com/gavinhsmith/titrmlib) | v0.2.0 |

```sh
git clone --recursive https://github.com/gavinhsmith/tinclib-config.git
# or, in an existing clone:
git submodule update --init
```

## Build
```sh
make            # -> bin/TINCLIBC.8xp
make relaunch   # -> tests/relaunch/bin/TSTRET.8xp + TINCHND.8xv (handoff test)
make clean
```
To run it, the calculator needs the [CE C libraries](https://github.com/CE-Programming/libraries/releases).

## Handoff (`TINCHND` appvar)
The byte layout is defined in [`src/handoff.h`](src/handoff.h). `tinclib`'s
`tinc_openConfig()` must write exactly that layout. TINCLIBC only acts on a
request whose result is still `PENDING`. It writes the result back only for the
nonce it read, then relaunches `return_to` with `os_RunPrgm`.

## Test
Host unit tests (plain C, ASan + UBSan):
```sh
make -C tests            # or: make host-test
make -C tests SANITIZE=  # toolchains without sanitizers (e.g. MinGW)
```

Emulator tests run in CEmu via `cemu-autotester`, which ships with the toolchain.
You need a TI-84 Plus CE ROM dump (not included; it's copyrighted) and
`clibs.8xg` from the [libraries release](https://github.com/CE-Programming/libraries/releases):
```sh
export AUTOTESTER_ROM=/path/to/ce.rom AUTOTESTER_LIBS_GROUP=/path/to/clibs.8xg
make test                                  # autotest.json: menu draws, [clear] exits
cemu-autotester tests/relaunch/autotest.json   # handoff -> os_RunPrgm -> TSTRET
```
The TINCLIBC screen CRCs are still `00000000` placeholders. Run each test once,
check the screen, and copy the CRC the autotester reports into `expected_CRCs`.

## CI/CD
`.github/workflows/ci.yml` runs the host tests, then builds TINCLIBC and the
relaunch test.
- **Emulator tests** run only when the `CE_ROM_BASE64` repo secret is set
  (`base64 -w0 ce.rom`). Otherwise they're skipped with a warning.
- **Releases**: push a `v*` tag to publish a GitHub release with `TINCLIBC.8xp` attached.
