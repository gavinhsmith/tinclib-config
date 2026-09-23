# AGENTS.md — tinclib-config

## What this repo is

**TINCLIBC.8xp** — the standalone TI-84 Plus CE program that owns all
board *configuration* state: Wi-Fi scanning/connecting, the saved network
profiles (as many slots as the firmware reports), the global
insecure-TLS-mode toggle, and (per the
handoff design) acting as the landing point when another program needs
setup done before it can use the network.

This program is built using `tinclib` (it's an app like any other from the
library's point of view) **plus its own admin-only commands** that are not
exposed through `tinclib.h` at all. Consumes `tinclib-protocol` as a
pinned dependency for the admin message definitions.

## Project status

Update this section when something lands or the release state changes.
**As of 2026-09-23** (protocol v0.3.0). No tags or releases yet.

**Dependencies** (git submodules under `lib/`):

| Submodule | Pin |
|---|---|
| `tinclib` | `phase-3` @ `9a5b26b` (protocol v0.3.0; not merged or tagged yet; switch to its tag once it exists) |
| `tinclib-protocol` | v0.3.0 |
| `titrmlib` | v0.2.0 |

tinclib has its own nested copy of the protocol at
`lib/tinclib/external/tinclib-protocol`. It isn't checked out or built here,
and CI fails if its pin differs from `lib/tinclib-protocol`. Bump both
together.

Upstream moved the protocol's `v0.2.0` tag (`9c21d6e` to `1c008bc`, same
files, rewritten history). Always pin by tag and check the commit a tag
points to when bumping; a moved tag leaves old pins pointing at commits no
branch contains.

**Working:**
- Talks to the board through tinclib:
  - status screen (Wi-Fi state, connected slot, RSSI, IP)
  - saved slots, as many as the board reports. The count comes from the
    HELLO reply (`wifi_slots`, 1..254). tinclib doesn't keep that reply,
    so `admin_slot_count()` sends its own HELLO (idempotent, same
    parameters as tinclib's). If tinclib ever exposes the count, use that
    instead.
    - Each slot is read with `WIFI_GET`, one frame per slot.
    - The list box shows 5 rows (`SLOTS_VISIBLE`). titrmlib's list keeps
      the selection in view and draws a scrollbar when there are more.
      The title shows the count, e.g. "Saved networks (12)".
    - Row buffers are sized for `TINC_WIFI_SLOTS_MAX` (254), about 14 KB
      of static data (32 KB total, of about 60 KB available).
    - Set (SSID + password + a hidden-network checkbox, sent as
      `TINC_WF_HIDDEN`) and forget. Hidden slots are marked "(hidden)".
      Slot range is checked by the board (`ERR_BAD_ARG`), not here.
  - connection test: Wi-Fi, then one HTTP GET to `http://example.com/`
- **Wi-Fi lock (0.2):** when STATUS reports `TINC_STATUSF_WIFI_LOCKED`,
  TINCLIBC treats it as "the board won't change profiles". The native PC
  build of the firmware uses this, since it can only use the PC's own
  connection.
  - The slot list is greyed out (palette index 0xB5) and titled
    "Saved networks (N) locked", and the status panel says so.
  - Selecting a slot opens a "Wi-Fi locked" notice instead of the
    set/forget menu.
  - The lock is only ever set on the board (build flag or switch), never
    from here, so there's no unlock path to add.
  - If the lock appears between a refresh and a save, WIFI_SET/FORGET
    return `ERR_LOCKED`, shown as tinclib's "Wi-Fi settings are locked",
    and the next refresh greys the list.
- Handoff: TINCHND is parsed, and the result is written back with the
  nonce echo. The layout is checked against tinclib's real code in
  `tests/test_interop.c`.

**Blocked or missing** (flag, don't work around):
- **Handoff return crashes** when TINCLIBC is bigger than the calling app;
  see the handoff rules below. `tests/handoff` (CEmu) is red in CI until
  this is fixed in CEdev's `os_RunPrgm` or in tinclib. That's expected: the
  user chose to leave it failing visibly.
- **Protocol 0.3 has only WIFI_GET/SET/FORGET as admin commands.** Scan,
  connect-now, the insecure-TLS toggle (`ERR_INSECURE_DISABLED` doesn't
  exist yet either), CA bundle updates, firmware info, HTTPS and time sync
  all need `tinclib-protocol` (and the firmware) first. The UI shows these
  as "not in protocol 0.3".
- **No 0.3 firmware yet.** `tinclib-firmware` `main` pins protocol 0.2
  (`1c008bc`). Pre-1.0, HELLO needs an exact MAJOR.MINOR match, so an older
  board answers `ERR_VERSION`, and the status screen says to update the
  firmware.
- **Real hardware:** tinclib's AGENTS.md reports that srldrvce supports only
  CDC, FTDI and PL2303 USB-serial bridges. CP210x and CH340 boards (the
  common ESP8266 dev boards) aren't seen by the calculator. That's a
  hardware decision for the user, not something to fix here.
- **Connect order:** the UI says the board picks the strongest saved
  network. Protocol 0.3's `WIFI_SET` comment says "first reachable slot,
  0 -> wifi_slots-1" instead. The two design docs disagree, so check with the user
  before changing either.
- **RSSI only for the connected slot:** STATUS carries RSSI for the current
  connection only, so other saved slots can't show signal strength until
  scan exists.
- **No captive-portal provisioning** in the firmware yet, so keypad entry
  is the only path.
- **Keypad symbols:** titrmlib can only type `. : - ? + " * / , ( ) ^` and
  space (and letters only in upper case, which is why TINCLIBC has its own
  field widget with a `[mode]` case toggle). Some Wi-Fi passwords can't be
  entered yet.

## Repo layout

| Path | What |
|---|---|
| `src/main.c` | titrmlib UI, event handling, reading and writing TINCHND |
| `src/admin.c/.h` | STATUS and admin commands over tinclib's internal `tinc_xfer()`; wipes the password buffer after sending |
| `src/handoff.c/.h` | Pure C99 TINCHND parse/write, mirroring tinclib's layout; no CE headers, so the host tests build it |
| `tests/test_*.c`, `tests/Makefile` | Host tests: handoff, admin payloads (link stubbed), and interop with tinclib's real handoff code on tinclib's host stubs |
| `autotest.json` | CEmu test: main screen with no board, `[clear]` exits |
| `tests/handoff/` | CEmu test: THANDOFF (a tinclib app) → TINCLIBC → back. Built from the repo root with `make handoff` |
| `tests/autotest.py` | Runs CEmu tests. Launches through AsmHook2 on arTIfiCE ROMs (reusing titrmlib's `tests/hw/run.py`) and renders failing screens to `tests/build/*/*.png` |

## Build, test, CI

- `make`, then `make handoff` (CEdev v15.0). `make -C tests` runs the host
  tests (`SANITIZE=` on MinGW, which can't link ASan).
- Emulator: `AUTOTESTER_ROM=… python tests/autotest.py autotest.json
  tests/handoff/autotest.json`. After a UI change, open the PNG before
  recording a new CRC: a CRC alone once "recorded" an OS error screen.
- CI (`.github/workflows/ci.yml`) runs the protocol-pin check and the host
  tests (ASan + UBSan), then builds and runs the emulator tests. The ROM
  (~4 MB, too big for a secret) is `ti-84ce.rom` in the private repo
  `gavinhsmith/ce-rom`, fetched with the `CE_ROM_TOKEN` secret (a read-only
  PAT). That ROM is OS 5.8.5, arTIfiCE-jailbroken, with clibs installed.
  Pushing a `v*` tag publishes a release with `TINCLIBC.8xp`.
- CEdev on Windows can't build sources reached through `..`. Any extra
  CEdev program builds from the repo root with its own `.mk` (see
  `tests/handoff/handoff.mk`); don't copy sources around.
- Commits and PRs are authored by the user only, with no AI co-author
  trailers.

## What TINCLIBC owns vs. what apps own — do not blur this line

**TINCLIBC owns (admin commands, `0x40+` in the protocol):**
- Wi-Fi scan
- Connect to / forget a saved slot (count set by the firmware)
- Setting a slot's SSID + password (write-only — no command ever reads a
  password back, and this program must never display one that was
  previously saved, since there's no way to retrieve it anyway)
- Marking a slot "hidden" (for SSIDs that don't show up in scans)
- Manual "connect to slot N now" (bypasses RSSI-based auto-selection)
- Global insecure-TLS-mode on/off toggle
- CA bundle update (mechanism TBD — flag if asked to implement without a
  clear firmware-side update path already existing)
- Firmware/version info display

**Regular apps own (via `tinclib`, not this program):**
- Their own API keys / auth headers — **never** stored or handled here.
  This was an explicit design decision: the ESP stores no per-service
  credentials, only Wi-Fi SSID+password. If asked to add "store an API
  key for app X" functionality here, that contradicts the settled design
  — flag it rather than implementing it.
- Any app-specific setup UI (e.g. "enter your API key") — TINCLIBC does
  not participate in this at all. It only handles Wi-Fi/connectivity
  setup, full stop.

## The handoff protocol (appvar-based) — implement exactly this shape

An app hands control to TINCLIBC and gets control back via a shared
appvar. TINCLIBC is one side of this exchange; `tinclib`'s
`tinc_openConfig()` is the other. Both sides must agree on the exact
layout — check `tinclib`'s current implementation before changing this,
since a mismatch here fails silently and confusingly.

```
TINCHND appvar  (byte layout: lib/tinclib/src/tinc_config.c, mirrored in src/handoff.h)
  magic + version        2B    'H', 1
  nonce                  4B    app-chosen; result must echo it back
  return_to              9B    the app's name (8 chars + NUL), display only
  action                 1B    SETUP_WIFI | TEST_CONN  (v1 action set)
  requirements           1B    bitfield: NEEDS_WIFI | NEEDS_TIME
  hint_len + hint_text   1B+var  short message shown to the user (<= 63)
  ---- filled in by TINCLIBC ----
  result                 1B    NONE (as written by the app) | OK | CANCELLED | FAILED
  nonce echo             4B
  detail_len + detail    1B+var  e.g. error code
```
Rules:
- **Nonce must be checked, not just echoed.** A stale result left over
  from an earlier abandoned request must be distinguishable from a fresh
  one — that's the entire purpose of the nonce. Don't skip this "because
  it's simple" — it's the mechanism that prevents acting on stale data.
- **No secrets ever go in this appvar.** Wi-Fi passwords go from the
  keypad straight into the admin command to the ESP; they never transit
  through `TINCHND`. This program must not write a password into any
  appvar, ever, even transiently.
- v1 action set is just `SETUP_WIFI` and `TEST_CONN`. Anything
  app-specific (API keys, per-app profiles) is explicitly **not** a
  TINCLIBC action — see above.
- After finishing, TINCLIBC writes the result and **exits**. It never
  relaunches `return_to`: the app comes back through the return callback
  `tinc_openConfig()` passed to `os_RunPrgm` (tinclib's hardware spike found
  that relaunching crashes the calculator). `tests/test_interop.c` checks the
  layout against tinclib's real code; `tests/handoff` runs the round trip
  in CEmu.
- **Known blocker (not fixed here):** in CEmu (OS 5.8.5), returning through
  that callback crashes the calculator (RAM Cleared) when the called
  program is bigger than the calling app. TINCLIBC (~38 KB) is bigger than
  most apps. It's in CEdev's `os_RunPrgm` return path or the OS, underneath
  tinclib's handoff design: fix it there, not with a workaround here.
  - **Measured:** with a 5.4 KB caller, a 2.7 KB stand-in TINCLIBC returns
    and one of 5.7 KB or more crashes.
  - **Confirmed:** padding the caller to 55 KB makes the real TINCLIBC
    round-trip correctly.
  - **Ruled out:** graphx, keypad scanning, run time, static data size and
    cursor-image RAM. `sprintf` only looked guilty because it made the
    stand-in bigger.
- Result TINCLIBC sends: `FAILED` (with the tinclib error code as detail)
  if there's no board; otherwise `OK` if Wi-Fi is connected (for
  `TEST_CONN`, if the connection test passed); otherwise `CANCELLED`.
- `TEST_CONN` should exercise Wi-Fi + time-sync + one real HTTPS request,
  since "connected" and "can actually complete an HTTPS request" are
  different questions on this hardware (see TLS constraints below) — a
  user should be able to tell the difference between "Wi-Fi joined" and
  "HTTPS actually works from here."

## Wi-Fi UX specifics

- **Slots (count set by the firmware), ranked at connect-time by strongest RSSI match**, not
  slot-priority order — this program's scan/connect UI should reflect
  that model (e.g. show signal strength per saved network, not a strict
  ordered list implying priority).
- Typing SSID/password on a calculator keypad is painful — if a
  captive-portal-style provisioning flow (temporary AP hosted by the ESP,
  configured from a phone) exists or is planned on the firmware side,
  TINCLIBC's UI should offer it as the primary path and keypad entry as a
  fallback, not the other way around. Confirm current firmware support
  before assuming this exists.
- Show signal strength (RSSI) on the connection-test screen — the
  `STATUS` message already carries this field.
- Networks detected as WPA2-Enterprise or captive-portal type (whatever
  the firmware can detect) should be marked unsupported in the scan list
  rather than silently failing to connect with no explanation.

## Insecure TLS mode toggle

- Off by default. This is the **only** place in the whole system where it
  can be turned on — no per-app or per-request-only override exists
  (to be enforced protocol-side via `ERR_INSECURE_DISABLED`, which
  `tinclib-protocol` v0.3.0 doesn't define yet; nor is there a toggle
  command). Do not add any other path to enable it.
- The UI here should make clear this is a global, security-relevant
  setting, not a per-connection convenience flag.

## Admin command access — unresolved, don't unilaterally decide

The protocol currently has **no access control** on admin (`0x40+`)
commands beyond "you have to be a program that speaks the protocol."
A physical-button confirmation on the ESP (press within ~10s of a
sensitive admin command) was discussed as the likely real fix, but it's
not decided. **This repo should not invent a software-only workaround**
(e.g. a PIN prompt) unilaterally — if this needs solving to ship a
feature, flag it back rather than picking a scheme that wasn't agreed on
in `tinclib-protocol`.

## Toolchain

Same as `tinclib`: CE C/C++ Toolchain (CEdev). Links `tinclib`
(`lib/tinclib`) plus this repo's own admin-command source, `src/admin.c`,
which reaches the link through tinclib's internal `tinc_xfer()`
(`tinc_internal.h`). `tinclib.h` exposes none of it, so apps that only use
the library never pull in admin code.
