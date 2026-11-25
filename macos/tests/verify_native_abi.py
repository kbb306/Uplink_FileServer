#!/usr/bin/env python3
"""Independent audit of native Intel method/field instructions in Uplink 1.6.9."""
from pathlib import Path
from verify_uplink_binary import get_slice
import sys
ANCHORS={
    0x94c10: '554889e58b7f0883ffff', # TaskManager::GetTargetProgramName
    0x94c1c: 'e87f6d02004885c0', # PID -> TaskWrapper branch
    0x1d4d0: '554889e5488b4718', # Interface::GetTaskManager, +0x18

    0x104e80: '488d3dbf131f00', # complete generation, done literal
    0x104e87: 'e8e2b61c00',     # puts call site
    0x104e8c: '488dbd80f8ffff', # exact return address
    0xE1B16: '488dbb28010000',      # Computer::databank at computer+0x128
    0xE8070: '554889e58b4748',      # DataBank::GetSize = [this+0x48]
    0xE8090: '554889e58b4718',      # DataBank::GetDataSize = [this+0x18]
    0xE80A0: '554889e541574156',    # DataBank::PutData(Data*)
    0xE91F0: '554889e54c8b4748',   # DataBank::GetMemoryIndex
    0xE8CF0: '554889e54863ce',      # DataBank::GetDataIndex
    0x5CA30: '554889e541574156',   # File Server native row enumerator
    0x5CAB8: '418b474c',           # file size field at +0x4c
    0x5CC26: 'e805feffff',         # helper -> row enumerator
    0x5C806: 'e825020000',         # browser row enumerator call #1
    0x5CF09: 'e822fbffff',         # browser row enumerator call #2
    0x5D52D: 'e8fef4ffff',         # browser row enumerator call #3
    0x5DA1E: 'e80df0ffff',         # browser row enumerator call #4
    0xEF8A8: '4c8db768020000',    # LanComputer::systems native DArray
    0x5C781: '8b053de03100',       # click global baseoffset read
    0x5CEA9: '8b0515d93100',       # draw global baseoffset read
    0x715F0: '554889e5488b7f08',  # GetComputerScreen accessor
    0xE3DE0: '554889e55350',      # GetComputer accessor
}
def main():
    img=get_slice(Path(sys.argv[1]).read_bytes())
    for rva,expected in ANCHORS.items():
        want=bytes.fromhex(expected)
        got=img[rva:rva+len(want)]
        if got!=want:
            raise SystemExit(f'FAIL native anchor {rva:#x}: expected {want.hex()}, got {got.hex()}')
    print(f'PASS: {len(ANCHORS)} exact native DataBank and file-browser instruction anchors')
if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('Usage: verify_native_abi.py UplinkExecutable')
    main()
