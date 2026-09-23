# AGENTS.md — tinclib-config

## What this repo is

**TINCLIBC.8xp** — the standalone TI-84 Plus CE program that owns all
board *configuration* state: Wi-Fi scanning/connecting, the 3 saved
network profiles, the global insecure-TLS-mode toggle, and (per the
handoff design) acting as the landing point when another program needs
setup done before it can use the network.

This program is built using `tinclib` (it's an app like any other from the
library's point of view) **plus its own admin-only commands** that are not
exposed through `tinclib.h` at all. Consumes `tinclib-protocol` as a
pinned dependency for the admin message definitions.

## What TINCLIBC owns vs. what apps own — do not blur this line

**TINCLIBC owns (admin commands, `0x40+` in the protocol):**
- Wi-Fi scan
- Connect to / forget a saved slot (3 slots max)
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
- `TEST_CONN` should exercise Wi-Fi + time-sync + one real HTTPS request,
  since "connected" and "can actually complete an HTTPS request" are
  different questions on this hardware (see TLS constraints below) — a
  user should be able to tell the difference between "Wi-Fi joined" and
  "HTTPS actually works from here."

## Wi-Fi UX specifics

- **3 slots, ranked at connect-time by strongest RSSI match**, not
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
  (enforced protocol-side via `ERR_INSECURE_DISABLED`; see
  `tinclib-protocol`). Do not add any other path to enable it.
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
