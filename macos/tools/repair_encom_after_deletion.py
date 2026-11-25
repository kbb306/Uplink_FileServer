#!/usr/bin/env python3
"""Safely restore displaced ENCOM UFS2 markers in a Uplink Mac SAV62 save.

Requires the last known-good partitioned save for a strict side-by-side check.
This script never alters either input file, and rejects any change beyond one
specified file deletion and movement of the six UFS2 marker memory indices.

Usage:
 python3 repair_encom_after_deletion.py --before Marbles-before.usr \
     --after Marbles-after.usr --output Marbles-repaired.usr \
     --deleted Monitor_Bypass
"""
import argparse
import hashlib
import struct
from pathlib import Path

MARKERS = {
  'UFS2.B.P00000003':90,
  'UFS2.E.P00000003':191,
  'UFS2.B.P00000018':192,
  'UFS2.E.P00000018':293,
  'UFS2.B.P0000001D':294,
  'UFS2.E.P0000001D':395,
}

def i32(b, p):return struct.unpack_from('<i',b,p)[0]
def u32(b, p):return struct.unpack_from('<I',b,p)[0]

def decode(path):
    b=path.read_bytes()
    if not b.startswith(b'REDSHRT2\0') or len(b)<34:raise ValueError(f'{path}: invalid header')
    checksum=hashlib.sha1(b[29:]).digest()
    swapped=b''.join(checksum[i:i+4][::-1] for i in range(0,20,4))
    if b[9:29]!=swapped:raise ValueError(f'{path}: invalid REDSHRT2 checksum')
    p=bytes(x^0x80 for x in b[29:])
    if not p.startswith(b'SAV62'):raise ValueError(f'{path}: unexpected save version')
    return p

def data_array(b,off):
    n=u32(b,off);p=off+4
    if n!=64:raise ValueError('File array size differs from validated ENCOM baseline')
    files={}
    for _ in range(n):
        ix=i32(b,p);p+=4
        if ix==-1:continue
        if ix<0 or ix>=n or ix in files:raise ValueError('Invalid file index')
        if u32(b,p)!=21:raise ValueError('Unexpected file object ID')
        p+=4
        count=u32(b,p);p+=4
        if not 1<=count<=128 or b[p+count-1]!=0:raise ValueError('Invalid title string')
        title=b[p:p+count-1].decode('latin1');p+=count
        attrs=b[p:p+24];p+=24
        typ,size,enc,comp,ver,soft=struct.unpack('<iiiifi',attrs)
        if not 1<=size<=396:raise ValueError('Invalid saved size')
        files[ix]=(title,attrs)
    return files,p

def memory_array(b,off):
    n=u32(b,off);p=off+4
    if n!=396:raise ValueError('Memory array size differs from validated ENCOM baseline')
    memory={}
    for _ in range(n):
        at=i32(b,p);p+=4
        if at==-1:continue
        if not 0<=at<n or at in memory:raise ValueError('Duplicate or invalid memory index')
        fi=i32(b,p);p+=4
        memory[at]=fi
    return memory,p

def locate(b):
    needle=b'\x40\x00\x00\x00\x00\x00\x00\x00\x15\x00\x00\x00'
    candidates=[];start=0
    while True:
        off=b.find(needle,start)
        if off<0:break
        start=off+1
        try:
            files,memstart=data_array(b,off)
            titlemap={name:fi for fi,(name,_) in files.items()}
            if any(name not in titlemap for name in MARKERS):continue
            mem,memend=memory_array(b,memstart)
            if memend>=len(b) or b[memend]!=0:continue
            candidates.append((files,memstart,mem,memend))
        except (ValueError,IndexError,struct.error):pass
    if len(candidates)!=1:raise ValueError(f'Expected exactly one ENCOM bank, found {len(candidates)}')
    return candidates[0]

def slot_locations(mem,slot):return sorted(p for p,s in mem.items() if s==slot)

def pack_memory(mem):
    data=bytearray(struct.pack('<I',396))
    for k in range(396):
        data+=struct.pack('<i',k if k in mem else -1)
        if k in mem:data+=struct.pack('<i',mem[k])
    return bytes(data)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for a in ('before','after','output'):ap.add_argument('--'+a,required=True,type=Path)
    ap.add_argument('--deleted',required=True)
    args=ap.parse_args()
    if args.output.resolve() in (args.before.resolve(),args.after.resolve()):
        raise ValueError('Output must not overwrite either original input')
    before=decode(args.before);after=decode(args.after)
    bf,bp,bm,be=locate(before);af,ap_,am,ae=locate(after)
    bnames={name:fi for fi,(name,_) in bf.items()};anames={name:fi for fi,(name,_) in af.items()}
    if args.deleted not in bnames or args.deleted in anames:raise ValueError('Expected deleted file not removed')
    removed_slot=bnames[args.deleted]
    expected={i:entry for i,entry in bf.items() if i!=removed_slot}
    if af!=expected:raise ValueError('File metadata changed beyond the expected deletion; refusing repair')
    expected_memory={position:slot for position,slot in bm.items() if slot!=removed_slot}
    for marker,want in MARKERS.items():
        slot=anames[marker]
        if slot_locations(bm,slot)!=[want]:raise ValueError(f'Baseline has displaced marker {marker}')
        old=slot_locations(am,slot)
        if len(old)!=1:raise ValueError(f'Marker {marker} is missing or duplicated')
        if int.from_bytes(af[slot][1][4:8],'little',signed=True)!=1:raise ValueError('Marker metadata invalid')
    for pos,slot in am.items():
        if slot not in [anames[name] for name in MARKERS] and expected_memory.get(pos)!=slot:
            raise ValueError('Non-marker storage differs from baseline; refusing repair')
    for pos,slot in expected_memory.items():
        if slot not in [anames[name] for name in MARKERS] and am.get(pos)!=slot:
            raise ValueError('Non-marker file missing or moved')
    marker_slots={anames[name] for name in MARKERS}
    new_mem={pos:slot for pos,slot in am.items() if slot not in marker_slots}
    for name,want in MARKERS.items():
        if want in new_mem:raise ValueError(f'Marker target {want} occupied by non-marker file')
        new_mem[want]=anames[name]
    if new_mem!=expected_memory:raise ValueError('Repair would not restore exact pre-deletion layout')
    blob=pack_memory(new_mem)
    if len(blob)!=ae-ap_:
        raise ValueError('New native memory encoding size unexpectedly differs')
    repaired=after[:ap_]+blob+after[ae:]
    # Keep exact original decoded payload length, data except six moved memory entries.
    if len(repaired)!=len(after):raise ValueError('Unexpected payload length')
    encoded=bytes(x^0x80 for x in repaired)
    digest=hashlib.sha1(encoded).digest()
    swapped=b''.join(digest[i:i+4][::-1] for i in range(0,20,4))
    raw=b'REDSHRT2\0'+swapped+encoded
    if decode_from_bytes(raw)!=repaired:raise ValueError('Checksum verification failed')
    rf,rp,rm,re=locate(repaired)
    if rf!=af or rm!=new_mem:raise ValueError('Verification after re-encoding failed')
    args.output.write_bytes(raw)
    print('REPAIR PASS | moved six native UFS2 boundary markers')
    print('All other saved file records, native memory references and non-bank save bytes preserved')
    print(f'Deleted file: {args.deleted} | saved DataBank capacity: 396 Gq')
    for name,gq in MARKERS.items():print(f'  {name} -> Gq {gq}')
    print(f'Repaired copy: {args.output}')

def decode_from_bytes(b):
    if not b.startswith(b'REDSHRT2\0'):raise ValueError('header')
    sha=hashlib.sha1(b[29:]).digest()
    if b[9:29]!=b''.join(sha[i:i+4][::-1] for i in range(0,20,4)):raise ValueError('checksum')
    return bytes(x^0x80 for x in b[29:])

if __name__=='__main__':main()
