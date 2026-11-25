#!/usr/bin/env python3
"""Verify exact File_Deleter vtable and native Tick/SetTarget/GUI anchors.
No modifications. Accepts thin or fat Mach-O x86_64 Uplink 1.6.9 image.
"""
from pathlib import Path
import struct,sys
from verify_uplink_binary import get_slice
ANCHORS={
  0x833a0:'554889e541574156', # FileDeleter::Tick starts, called by native scheduler
  0x833c8:'488b07ff5038',     # virtual IsInterfaceVisible
  0x83423:'41c744243c01000000', # OFF->INPROGRESS
  0x835e9:'e8c2560600',       # dangerous native DataBank::RemoveData call
  0x835ee:'41807c244000',     # subsequent native branch into repacking loop
  0x82e80:'554889e541574156', # virtual FileDeleter::SetTarget
  0x82edd:'8b404c8d04c0',     # required ticks = 9 * file size
  0x82ee3:'4189442444',       # numticksrequired at object+0x44
  0x833fc:'418b44243c',       # status at +0x3c
  0x83533:'4189442448',       # progress at +0x48
  0x94bf0:'554889e58b7f08',  # TaskManager::GetTargetProgram
  0x94c10:'554889e58b7f08',  # TaskManager::GetTargetProgramName
  0xb9400:'554889e5',        # SvbLookupPID
  0x836e0:'554889e5',        # FileDeleter reset target helper
  0xa61a0:'554889e5',        # GUI caption updater
  0xa0a20:'554889e5',        # EclGetButton
  0xa1750:'554889e5',        # EclDirtyButton
  0x14950:'554889e5',        # Game::GetWorld
  0x15f920:'554889e5',       # World::GetPlayer
  0x14ed40:'554889e5',       # Player::IsConnected
}
VTABLE=[0x82cc0,0x82cd0,0x83390,0x833a0,0x83830,
        0x83a40,0x83b40,0x83c50,0x82e80,0x82d00]
def main(file):
    b=get_slice(Path(file).read_bytes())
    for off,hx in ANCHORS.items():
        x=bytes.fromhex(hx)
        if b[off:off+len(x)]!=x:
            raise SystemExit(f'FAIL animation anchor {off:#x}: '+b[off:off+len(x)].hex()+' expected '+hx)
    assert struct.unpack_from('<Q',b,0x36cc10)[0]==0
    assert struct.unpack_from('<Q',b,0x36cc18)[0]==0x10036cc70
    for i,rva in enumerate(VTABLE):
        got=struct.unpack_from('<Q',b,0x36cc20+i*8)[0]
        if got!=0x100000000+rva:raise SystemExit(f'FAIL vtable slot {i}: {got:#x}')
    print('PASS: '+str(len(ANCHORS))+' native timing/UI anchors + 10 vtable slots')
if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('usage: verify_vanilla_animation.py UplinkExecutable')
    main(sys.argv[1])
