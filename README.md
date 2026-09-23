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
You need a TI-84 Plus CE ROM dump (not included; it's copyrighted) with the
[CE C libraries](https://github.com/CE-Programming/libraries/releases) installed,
or set `AUTOTESTER_LIBS_GROUP` to `clibs.8xg`:
```sh
make && make relaunch
AUTOTESTER_ROM=/path/to/ce.rom python tests/autotest.py autotest.json tests/relaunch/autotest.json
```
- `autotest.json`: the menu draws and `[clear]` exits.
- `tests/relaunch/autotest.json`: handoff → `os_RunPrgm` → TSTRET, which
  checks that the result came back for its nonce.

`tests/autotest.py` launches through AsmHook2 on arTIfiCE-jailbroken ROMs
(OS 5.5+ has no `Asm(`), reusing titrmlib's runner. Failing screens are saved
as PNGs under `tests/build/`. To re-record a CRC after a UI change, check the
PNG, then copy the CRC the autotester reports into `expected_CRCs`.

## CI/CD
`.github/workflows/ci.yml` runs the host tests, then builds TINCLIBC and the
relaunch test.
- **Emulator tests** run only when the `CE_ROM_TOKEN` repo secret is set.
  Otherwise they're skipped with a warning. The ROM is ~4 MB, far over the
  48 KB secret limit, so it lives as `ti-84ce.rom` in the private repo
  `gavinhsmith/ce-rom`. `CE_ROM_TOKEN` is a fine-grained PAT with read-only
  *Contents* access to that one repo.
- **Releases**: push a `v*` tag to publish a GitHub release with `TINCLIBC.8xp` attached.
