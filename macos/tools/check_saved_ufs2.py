#!/usr/bin/env python3
"""Check native save wrapper/checksum and search for ENCOM UFS2 marker titles.

This is a NON-MUTATING presence check, NOT a complete DataBank deserializer.
A PASS means six correctly encoded titles survived save serialization;
it does not establish precise file placement or browser isolation.
"""
import argparse
import hashlib
from pathlib import Path
import struct

MAGIC = b'REDSHRT2\0'
TITLES = tuple(f'UFS2.{boundary}.P{index:08X}' for index in (3, 24, 29) for boundary in ('B', 'E'))


def decode(data: bytes) -> bytes:
    if not data.startswith(MAGIC) or len(data) < 34:
        raise ValueError('Not a REDSHRT2-wrapped Uplink save')
    digest = hashlib.sha1(data[29:]).digest()
    want = b''.join(digest[i:i + 4][::-1] for i in range(0, 20, 4))
    if data[9:29] != want:
        raise ValueError('REDSHRT2 checksum mismatch; DO NOT use this save')
    decoded = bytes(x ^ 0x80 for x in data[29:])
    if not decoded.startswith(b'SAV62'):
        raise ValueError('Unexpected payload type (expected SAV62)')
    return decoded


def marker_counts(payload: bytes):
    return {title: payload.count(struct.pack('<I', len(title) + 1) + title.encode('ascii') + b'\0')
            for title in TITLES}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('save', type=Path, help='Path to Marbles.usr AFTER quitting Uplink')
    parser.add_argument('--expect-encom', action='store_true', help='Require each ENCOM marker exactly once')
    args = parser.parse_args()
    raw = args.save.read_bytes()
    decoded = decode(raw)
    counts = marker_counts(decoded)
    print('REDSHRT2 checksum: PASS')
    print('Payload version: SAV62')
    print('SHA256:', hashlib.sha256(raw).hexdigest())
    for title, n in counts.items():
        print(f'{title}: {n} serialized title(s)')
    if args.expect_encom:
        if all(n == 1 for n in counts.values()):
            print('ENCOM UFS2 TITLE PRESENCE: PASS (six titles found; NOT full structural validation)')
        else:
            raise SystemExit('ENCOM UFS2 TITLE PRESENCE: FAIL; preserve the save for investigation')
    else:
        print('Presence check only; pass --expect-encom to require all six markers')


if __name__ == '__main__':
    main()
