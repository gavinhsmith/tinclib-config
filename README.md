# ti-84-ce-project
A template for a C project for the TI-84 Plus CE w/ CICD, testing features, etc.

## Setup
Install the [CE C toolchain](https://ce-programming.github.io/toolchain/) (v15.0) and put `CEdev/bin` on your `PATH`.

## Build
```sh
make          # -> bin/DEMO.8xp
make debug    # debug build
make clean
```
Rename the program via `NAME` in `makefile` (and update `autotest.json` to match).

## Test
Tests run in CEmu via `cemu-autotester` (ships with the toolchain), driven by `autotest.json`.
You need a TI-84 Plus CE ROM dump (not included, it's copyrighted):
```sh
AUTOTESTER_ROM=/path/to/ce.rom make test
```
To add a screen check: add a `hashWait|N` step and a hash entry with a placeholder CRC,
run the test, and copy the actual CRC the autotester reports into `expected_CRCs`.
See the [autotester docs](https://github.com/CE-Programming/CEmu/tree/master/tests/autotester).

If your program uses the CE libraries (graphx, fileioc, ...), also set
`AUTOTESTER_LIBS_GROUP` to a `clibs.8xg` from the [libraries release](https://github.com/CE-Programming/libraries/releases).

## CI/CD
`.github/workflows/ci.yml` builds on every push/PR and uploads `bin/*.8xp` as an artifact.
- **Tests** run when the `CE_ROM_BASE64` repo secret is set (`base64 -w0 ce.rom`), otherwise skipped with a warning.
- **Releases**: push a `v*` tag to publish a GitHub release with the `.8xp` attached.
