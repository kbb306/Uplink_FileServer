#!/usr/bin/env python3
"""Regression fixture tests for REDSHRT2 decoding and marker extraction."""
from pathlib import Path
import sys
import hashlib
import struct
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from check_saved_ufs2 import MAGIC, TITLES, decode, marker_counts

def wrap(payload):
    encoded = bytes(x ^ 0x80 for x in payload)
    digest = hashlib.sha1(encoded).digest()
    return MAGIC + b''.join(digest[i:i + 4][::-1] for i in range(0, 20, 4)) + encoded

p = b'SAV62fixture' + b''.join(struct.pack('<I', len(s) + 1) + s.encode() + b'\0' for s in TITLES)
raw = wrap(p)
assert decode(raw) == p
assert all(x == 1 for x in marker_counts(p).values())
bad = bytearray(raw); bad[-1] ^= 0x02
try:
    decode(bad)
    raise AssertionError('Bad checksum unexpectedly accepted')
except ValueError:
    pass
assert all(x == 0 for x in marker_counts(b'SAV62') .values())
print('PASS: REDSHRT2 checksum, six marker titles, corruption detection')
