#!/usr/bin/env python3
"""Verify exact known Intel Uplink 1.6.9 Mach-O parser and native writer anchors.
Accept the fat (universal) or thin executable image. Read only.
"""
import hashlib
import struct
import sys
from pathlib import Path

EXPECTED_FAT_SHA = '338507721bebdd724cb81d37a300ba67caf59088acab2da2e43234427903d1e2'
EXPECTED_UUID = bytes.fromhex('be676540d8cc326d8e698a25e57cd10d')
ANCHORS = {
    0x94c10: '554889e58b7f0883ffff',
    0x94c1c: 'e87f6d02004885c0',
    0x1d4d0: '554889e5488b4718',

    0xe8050: '554889e54883c738',     # native DataBank::SetSize wrapper (memory DArray)
    0xe8290: '554889e541574156',     # native DataBank::PutData(Data*,int)
    0xe98d0: '554889e55350',         # native Data constructor
    0xe9a20: '554889e5415653',       # native Data::SetTitle
    0x2d01a2: 'ff25f0820900',        # native ::operator new stub
    0xe1c3b: '4883c324',             # Computer::SetName destination +0x24
    0xe1c9f: '4983c664',             # Computer::SetCompanyName destination +0x64
    0xe1d0a: '4881c3a4000000',       # Computer::SetIP destination +0xa4
    0x10453a: '488d354fa21f00',           # lea MAINSERVER
    0x104541: 'e8eec01c00',               # call strstr
    0x104546: '4885c0',                   # return site
    0x10456c: 'e8ff1d0000',               # native GenerateMainServer
    0x1047d6: '4c8bb588faffff',           # common epilogue after generator
    0x1047e7: '83f8ff0f8515fcffff',       # cmp index,-1 ; jne apply data
    0x10442e: '8b8d68faffff894828',       # DATA1 stack -> node field +0x28
    0x104d58: 'e84333feff',               # last native DataBank::PutData from file loop
    0x104e80: '488d3dbf131f00',           # lea literal "done" after DataBank and records
    0x104e87: 'e8e2b61c00',               # call stub _puts, targeted post-load trigger
    0x104e8c: '488dbd80f8ffff',           # return instruction after _puts

    0x104405: '4898488b95a8faffff',       # apply native data1-3
    0x106370: '554889e54157',             # native main server generator
    0x10675e: '41c744240805000000',       # write TYPE 5 to +0x8
    0xe77f0:  '554889e589b7c8000000',     # PasswordScreen::SetNextPage
    0xe3d20:  '554889e5415653',           # GenericScreen::SetMainTitle
    0x1067ab: '488b4b204c8924c1',       # save pointer in DArray at +0x20
    0xe1f01:  '488b8318010000',           # computer screens ptr at +0x118
    0x5c76f:  '488d35bbf72800', # FileClick exact filename parse format
    0x5c77c:  'e8713e2700',     # FileClick exact sscanf stub
    0x5c781:  '8b053de03100',   # FileClick global baseoffset
    0x5ce94:  '488d3596f02800', # FileDraw exact filename parse format
    0x5cea4:  'e849372700',     # FileDraw exact sscanf stub
    0x5cea9:  '8b0515d93100',   # FileDraw global baseoffset
    0xef8a8:  '4c8db768020000', # LanComputer::systems DArray at +0x268
    0x715d3:  '8b7314',         # RemoteInterface::currentScreenIndex
    0x14934:  '488b4708',       # Game::GetInterface
    0x1d494:  '488b4710',       # Interface::GetRemoteInterface
    0x715f0:  '554889e5488b7f08', # RemoteInterface::GetComputerScreen
    0xe3de0:  '554889e55350',   # ComputerScreen::GetComputer
    0x5ceb5:  '488d05ecd63100', # UI current global game pointer
}

def get_slice(data):
    if data[:4] == b'\xca\xfe\xba\xbe':
        n=struct.unpack_from('>I',data,4)[0]
        for i in range(n):
            cpu,sub,off,length,align=struct.unpack_from('>IIIII',data,8+20*i)
            if cpu==0x01000007:
                print(f'Universal executable: Intel x86_64 at offset 0x{off:x}')
                if off+length>len(data): raise ValueError('Bad fat offset')
                return data[off:off+length]
        raise ValueError('No x86_64 slice')
    return data

def main(path):
    raw=Path(path).read_bytes()
    digest=hashlib.sha256(raw).hexdigest()
    print('SHA256:',digest)
    img=get_slice(raw)
    if struct.unpack_from('<I',img,0)[0]!=0xfeedfacf: raise ValueError('Not Mach-O 64-bit')
    if struct.unpack_from('<I',img,4)[0]!=0x01000007:raise ValueError('Not Intel 64-bit')
    ncmds,sizeofcmds=struct.unpack_from('<II',img,16)
    offset=32;found=None
    for i in range(ncmds):
        cmd,sz=struct.unpack_from('<II',img,offset)
        if cmd==0x1b:found=img[offset+8:offset+24]
        offset+=sz
    if found!=EXPECTED_UUID:raise ValueError(f'Unexpected LC_UUID: {found!r}')
    for off,expected in ANCHORS.items():
        want=bytes.fromhex(expected)
        if img[off:off+len(want)]!=want:raise ValueError(f'Unexpected instructions at RVA {off:#x}')
    print('PASS: expected LC_UUID and all',len(ANCHORS),'x86_64 parser, generator and UI anchors')
    if raw[:4] == b'\xca\xfe\xba\xbe' and digest!=EXPECTED_FAT_SHA:
        print('WARNING: UUID and anchors match, but full universal-file hash differs; review changes.')

if __name__=='__main__':
    if len(sys.argv)!=2:
        print('Usage: verify_uplink_binary.py PATH_TO_UPLINK_EXECUTABLE',file=sys.stderr)
        raise SystemExit(2)
    main(sys.argv[1])
