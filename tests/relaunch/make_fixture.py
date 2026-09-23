"""Writes a fresh TINCHND request (layout: src/handoff.h) returning to TSTRET."""
import os
import struct
import sys

hint = b"Relaunch test"
data = (
    bytes([0x54, 1])                    # magic 'T', version
    + struct.pack("<I", 0x1234ABCD)     # nonce
    + b"TSTRET".ljust(9, b"\0")         # return_to
    + bytes([1, 0x01])                  # SETUP_WIFI, NEEDS_WIFI
    + bytes([len(hint)]) + hint
    + bytes([0, 0])                     # result PENDING, detail_len 0
)
os.makedirs(os.path.dirname(sys.argv[1]), exist_ok=True)
open(sys.argv[1], "wb").write(data)
