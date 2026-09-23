# tinclib-config

**TINCLIBC.8xp**: the TI-84 Plus CE program that owns TINCLIB board configuration
(Wi-Fi slots, the global insecure-TLS toggle) and is where apps hand off
to when they need setup. See [AGENTS.md](AGENTS.md) for the design rules.

Status: talks to the board through `tinclib`.
- Status screen: Wi-Fi state, slot, signal (RSSI) and IP.
- Saved networks: the 3 slots, with set (SSID + masked password) and forget.
- Connection test: Wi-Fi plus one HTTP GET.
- Not in protocol 0.1 yet: scan, hidden networks, connect-now, the
  insecure-TLS toggle, and HTTPS/time sync.
- Handoff return is blocked: see [Handoff](#handoff-tinchnd-appvar).

## Setup
Install the [CE C toolchain](https://ce-programming.github.io/toolchain/) (v15.0) and put `CEdev/bin` on your `PATH`.
Dependencies are pinned git submodules under `lib/`:

| Submodule | Tag |
|---|---|
| [tinclib](https://github.com/gavinhsmith/tinclib) | `phase-1` @ `28c2a3a` (v0.1.0 not tagged yet) |
| [tinclib-protocol](https://github.com/gavinhsmith/tinclib-protocol) | v0.1.0 |
| [titrmlib](https://github.com/gavinhsmith/titrmlib) | v0.2.0 |

tinclib has its own nested copy of the protocol, which isn't checked out or
built here. CI fails if it pins a different commit than `lib/tinclib-protocol`.

```sh
git clone --recursive https://github.com/gavinhsmith/tinclib-config.git
# or, in an existing clone:
git submodule update --init
```

## Build
```sh
make            # -> bin/TINCLIBC.8xp
make handoff    # -> bin/handoff/THANDOFF.8xp, a tinclib app for the handoff test
make clean
```
To run it, the calculator needs the [CE C libraries](https://github.com/CE-Programming/libraries/releases)
(graphx, keypadc, fileioc, usbdrvce, srldrvce).

Typing on the keypad: `[alpha]` types one letter and `[2nd][alpha]` locks alpha.
`[mode]` switches letters between lower and upper case. Only the symbols
`. : - ? + " * / , ( ) ^` and space can be typed.

## Handoff (`TINCHND` appvar)
tinclib owns the byte layout (`lib/tinclib/src/tinc_config.c`), and
[`src/handoff.h`](src/handoff.h) mirrors it. TINCLIBC only acts on a request
whose result is still `NONE`. It writes back the result and a nonce echo, then
**exits**. The app comes back through its own `os_RunPrgm` callback.

The result is:
- `OK` if Wi-Fi is up (for `TEST_CONN`, if the test passed)
- `FAILED` with the error code if there's no board
- `CANCELLED` otherwise

**Known blocker:** in CEmu (OS 5.8.5), that callback return crashes the
calculator (RAM Cleared) when the called program is bigger than the caller.
TINCLIBC is about 38 KB, so `tests/handoff` fails until this is fixed in
tinclib or CEdev.

## Test
Host tests (plain C, ASan + UBSan):
- `test_handoff`: parsing and writing the handoff appvar
- `test_admin`: admin command payloads, with the link stubbed
- `test_interop`: tinclib's real `tinc_openConfig()` and
  `tinc_takeSetupResult()` against our parser, on tinclib's host stubs
```sh
make -C tests            # or: make host-test
make -C tests SANITIZE=  # toolchains without sanitizers (e.g. MinGW)
```

Emulator tests run in CEmu via `cemu-autotester`, which ships with the toolchain.
You need a TI-84 Plus CE ROM dump (not included; it's copyrighted) with the
[CE C libraries](https://github.com/CE-Programming/libraries/releases) installed,
or set `AUTOTESTER_LIBS_GROUP` to `clibs.8xg`:
```sh
make && make handoff
AUTOTESTER_ROM=/path/to/ce.rom python tests/autotest.py autotest.json tests/handoff/autotest.json
```
- `autotest.json`: the menu draws (no board in the emulator) and `[clear]` exits.
- `tests/handoff/autotest.json`: THANDOFF calls `tinc_openConfig()`, TINCLIBC
  shows its hint, `[clear]` exits, and THANDOFF's `tinc_init()` reports
  `FAILED` (no board).

`tests/autotest.py` launches through AsmHook2 on arTIfiCE-jailbroken ROMs
(OS 5.5+ has no `Asm(`), reusing titrmlib's runner. Failing screens are saved
as PNGs under `tests/build/`. To re-record a CRC after a UI change, check the
PNG, then copy the CRC the autotester reports into `expected_CRCs`.

## CI/CD
`.github/workflows/ci.yml` checks the protocol pins match and runs the host
tests. It then builds TINCLIBC and THANDOFF and runs the emulator tests.
- **Emulator tests** run only when the `CE_ROM_TOKEN` repo secret is set.
  Otherwise they're skipped with a warning. The ROM is ~4 MB, far over the
  48 KB secret limit, so it lives as `ti-84ce.rom` in the private repo
  `gavinhsmith/ce-rom`. `CE_ROM_TOKEN` is a fine-grained PAT with read-only
  *Contents* access to that one repo.
- **Releases**: push a `v*` tag to publish a GitHub release with `TINCLIBC.8xp` attached.
