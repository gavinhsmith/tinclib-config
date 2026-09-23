#!/usr/bin/env python3
"""Runs CEmu autotests, handling ROMs jailbroken with arTIfiCE.

    python tests/autotest.py autotest.json tests/relaunch/autotest.json

The autotester's "action|launch" types Asm(prgmNAME), which OS 5.5+ removed.
On a ROM with AsmHook2, launch mode and key sequence come from titrmlib's hw
test runner (lib/titrmlib/tests/hw/run.py), so both repos launch the same way.
Needs AUTOTESTER_ROM (and AUTOTESTER_LIBS_GROUP if the ROM lacks clibs).
Failing hashes are dumped and rendered to tests/build/<name>/*.png.
"""
import glob
import json
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "lib", "titrmlib", "tests", "hw"))
import run  # noqa: E402  (titrmlib's runner: launch_mode, artifice_launch, dump_to_png)


def main(paths):
    rom = run.find_rom(None)
    mode = run.launch_mode(None, rom)
    autotester = run.find_autotester()
    failed = False
    for path in paths:
        src = os.path.abspath(path)
        with open(src, encoding="utf-8") as f:
            cfg = json.load(f)
        base = os.path.dirname(src)
        cfg["transfer_files"] = [os.path.join(base, p) for p in cfg["transfer_files"]]
        if mode == "artifice":
            seq = []
            for step in cfg["sequence"]:
                seq += run.artifice_launch(cfg["target"]["name"]) if step == "action|launch" else [step]
            cfg["sequence"] = seq

        name = os.path.basename(base) if base != ROOT else "main"
        out = os.path.join(ROOT, "tests", "build", name)
        os.makedirs(out, exist_ok=True)
        for old in glob.glob(os.path.join(out, "*")):
            os.remove(old)
        resolved = os.path.join(out, "autotest.json")
        with open(resolved, "w", encoding="utf-8") as f:
            json.dump(cfg, f, indent=2)

        print(f"== {os.path.relpath(src, ROOT)} ({mode} launch)", flush=True)
        # -d dumps the memory behind each failing hash (into the config's dir).
        r = subprocess.run([autotester, "-d", resolved], cwd=out)
        for dump in glob.glob(os.path.join(out, "*_dump.bin")):
            print("   ", os.path.relpath(run.dump_to_png(dump), ROOT))
        failed |= r.returncode != 0
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["autotest.json"]))
