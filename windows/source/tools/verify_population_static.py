#!/usr/bin/env python3
"""Offline signature/architecture checks, NOT an in-game test."""
import hashlib, pathlib, struct, subprocess, sys
root = pathlib.Path(__file__).resolve().parents[1]
exe = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path('/mnt/data/Uplink.exe')
dll = root / 'bin' / 'UplinkFileServer.dll'
assert exe.is_file() and dll.is_file()
b = exe.read_bytes()
assert hashlib.sha256(b).hexdigest() == '837eb6465eb9538018f9b213bbd8d3f385fafd22182626ba6f21858223a34087'
expected = {
  0x00022570: '83 c1 14 e9', 0x00022580: '8b 41 18 c3',
  0x000b3f00: '8b 41 08 c3', 0x00022230: '56 8b f1 e8',
  0x00022ee0: '8b 44 24 04', 0x00022f10: '53 56 8b d9',
  0x00022660: '56 8b 74 24', 0x000226e0: '53 8b 5c 24 08',
  0x000225c0: '51 55 56 57',
  0x00033120: '51 8b 44 24 10 8b 4c 24',
  0x00052dd0: '6a ff 68 2b 02 56 00',
}
# Native MainServer code reads Computer::companyname at +0x54.
assert b[0x000537C5:0x000537C8] == bytes.fromhex('8d 47 54')
# Native Data constructor initializes version at +0x54, fields +0x44..0x58.
assert b[0x00022250:0x00022255] == bytes.fromhex('33 c0 d9 5e 54')
for rva, sig in expected.items():
    raw = bytes.fromhex(sig)
    assert b[rva:rva+len(raw)] == raw, f'Bad function fingerprint: RVA {rva:#x}'
for rva in (0x000333ED, 0x000339EC, 0x00033FBA, 0x00034386):
    assert b[rva] == 0xe8
    displacement = struct.unpack_from('<i', b, rva + 1)[0]
    assert rva + 5 + displacement == 0x00033120, f'Unexpected scanner call: {rva:#x}'
d = dll.read_bytes()
assert d[:2] == b'MZ'
pe = struct.unpack_from('<I', d, 0x3c)[0]
assert d[pe:pe+4] == b'PE\0\0'
assert struct.unpack_from('<H',d,pe+4)[0] == 0x14c
assert struct.unpack_from('<H',d,pe+24)[0] == 0x10b
assert b'KERNEL32.dll' in d
assert b'GetTickCount' in d
assert b'POPULATE: native files created' in d
print('PASS: exact Uplink Steam 1.55 SHA-256')
print('PASS: eleven native function fingerprints and four row calls')
print('PASS: built patch is a PE32 x86 DLL')
print('PASS: GetTickCount import and population logging embedded')
print('NOT TESTED: Uplink runtime, UI, file transfers, native save/reload')
