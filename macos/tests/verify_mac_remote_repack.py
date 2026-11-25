#!/usr/bin/env python3
"""Static proof of vanilla Mac 1.6.9 remote-only post-delete repack.

Checks native FileDeleter::Tick call to RemoveData, remote flag branch, and
subsequent loop's native GetDataFile, RemoveDataFile and PutData invocations.
Does not modify the target executable, install or run a game modification.
"""
from pathlib import Path
import sys
from verify_uplink_binary import get_slice
anchors={
  0x835e9:'e8c2560600', # DataBank::RemoveData, native deletion
  0x835ee:'41807c244000', # cmp byte [r12+0x40],0 = remote flag
  0x835f4:'0f84a9000000', # skip repack when not remote
  0x835ff:'e86c4a0600', # DataBank::GetSize (native bank size)
  0x83640:'e80b5a0600', # DataBank::GetDataFile
  0x83675:'e876560600', # DataBank::GetDataIndex
  0x8367f:'e8ac560600', # DataBank::RemoveDataFile
  0x8368e:'e8fd4b0600', # DataBank::PutData(new Data, new index)
}
def main(path):
    b=get_slice(Path(path).read_bytes())
    for off,hx in anchors.items():
        want=bytes.fromhex(hx)
        if b[off:off+len(want)]!=want:
            raise SystemExit(f'FAIL native auto-repack anchor {off:#x}: {b[off:off+len(want)].hex()} != {want.hex()}')
    print('PASS: exact Mac 1.6.9 native remote FileDeleter auto-repacking loop; local skip confirmed')
if __name__=='__main__':
    if len(sys.argv)!=2:sys.exit('usage: verify_mac_remote_repack.py Uplink-executable')
    main(sys.argv[1])
