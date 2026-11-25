#!/usr/bin/env python3
"""Read-only structural validator for ENCOM UFS2 on Mac Uplink 1.6.9 SAV62.

Accepts populated private partitions. Checks REDSHRT2 SHA1, exact saved marker
Gq indices, all native file-slot references, contiguous extents and confinement
of every file to root or exactly one private volume. No save modification.
"""
from pathlib import Path
import argparse
import hashlib
import struct

ROOT = 90
SIZE = 396
MARKERS = {3: (90, 191), 24: (192, 293), 29: (294, 395)}

def i32(b, p): return struct.unpack_from('<i', b, p)[0]
def u32(b, p): return struct.unpack_from('<I', b, p)[0]

def payload(path):
    b = Path(path).read_bytes()
    if not b.startswith(b'REDSHRT2\0') or len(b) < 34:
        raise ValueError('Not a REDSHRT2 save')
    sha = hashlib.sha1(b[29:]).digest()
    expect = b''.join(sha[i:i+4][::-1] for i in range(0,20,4))
    if b[9:29] != expect:
        raise ValueError('REDSHRT2 checksum mismatch')
    d = bytes(x ^ 0x80 for x in b[29:])
    if not d.startswith(b'SAV62'):
        raise ValueError('Unexpected save payload version (expected SAV62)')
    return d

def data_array(b, off):
    slots = u32(b, off);p = off+4
    if not 1 <= slots <= 4096: raise ValueError('Invalid file slot count')
    files = {}
    for _ in range(slots):
        idx = i32(b,p);p += 4
        if idx == -1: continue
        if not 0 <= idx < slots or idx in files: raise ValueError('Bad file index')
        typ_id = u32(b,p);p += 4
        if typ_id != 21: raise ValueError('Invalid DATA object type')
        slen = u32(b,p);p += 4
        if not 1 <= slen <= 128 or p+slen+24>len(b):
            raise ValueError('Bad DATA title length')
        title = b[p:p+slen];p += slen
        if title[-1] != 0:raise ValueError('Unterminated DATA title')
        typ,size,enc,comp,vers,software = struct.unpack_from('<iiiifi',b,p);p += 24
        if not 1 <= size <= 400 or not -100 < typ < 100:
            raise ValueError('Invalid DATA type/capacity')
        if not 0 <= enc < 100 or not 0 <= comp < 100:
            raise ValueError('Invalid encryption/compression')
        files[idx] = dict(title=title[:-1].decode('latin-1'),typ=typ,
                          size=size,enc=enc,comp=comp,version=vers,software=software)
    return files,p,slots

def memory_array(b,off):
    n=u32(b,off);p=off+4
    if not 1 <= n <= 20000:raise ValueError('Invalid memory capacity')
    memory={}
    for _ in range(n):
        at=i32(b,p);p+=4
        if at==-1:continue
        if at<0 or at>=n or at in memory:raise ValueError('Bad/duplicate memory index')
        fi=i32(b,p);p+=4
        memory[at]=fi
    return memory,p,n

def check_at(d,off):
    files,p,slots=data_array(d,off)
    if sum(info['title'].startswith('UFS2.') for info in files.values()) != 6:
        raise ValueError('Wrong number of UFS2 file records')
    mem,q,n=memory_array(d,p)
    if q>=len(d) or d[q] != 0:raise ValueError('DataBank formatted/invalid')
    if n!=SIZE:raise ValueError('Unexpected DataBank capacity')
    members={idx:[] for idx in files}
    for at,fi in sorted(mem.items()):
        if fi not in members:raise ValueError('Dangling memory -> DATA reference')
        members[fi].append(at)
    for fi,info in files.items():
        blocks=members[fi]
        if len(blocks)!=info['size'] or not blocks or blocks[-1]-blocks[0]+1!=len(blocks):
            raise ValueError(f'Noncontiguous/incorrect Gq allocation in DATA slot {fi}')
    name_to_fi={}
    for fi,info in files.items():
        name=info['title']
        if name in name_to_fi and name.startswith('UFS2.'):
            raise ValueError('Duplicate guard title')
        name_to_fi[name]=fi
    expected_names=set()
    for node,(start,end) in MARKERS.items():
        for boundary,position in [('B',start),('E',end)]:
            name=f'UFS2.{boundary}.P{node:08X}'
            expected_names.add(name)
            if name not in name_to_fi:raise ValueError('Missing guard '+name)
            fi=name_to_fi[name];info=files[fi]
            if members[fi]!=[position] or info['size']!=1 or info['typ']!=1 or info['enc'] or info['comp']:
                raise ValueError('Moved/malformed guard '+name)
    if {info['title'] for info in files.values() if info['title'].startswith('UFS2.')}!=expected_names:
        raise ValueError('Unexpected UFS2 metadata')
    regions={'ROOT':(0,ROOT),'NODE3':(91,191),'NODE24':(193,293),'NODE29':(295,395)}
    grouped={label:[] for label in regions}
    for fi,info in files.items():
        if info['title'] in expected_names:continue
        start=members[fi][0];end=members[fi][-1]+1
        labels=[label for label,(lo,hi) in regions.items() if lo<=start and end<=hi]
        if len(labels)!=1:raise ValueError(f'DATA slot {fi} crosses a partition/guard')
        grouped[labels[0]].append((start,info))
    return dict(off=off,slots=slots,files=files,used=len(mem),regions=grouped)

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('save',type=Path)
    args=ap.parse_args()
    d=payload(args.save)
    # DArray's file array is 64 slots in this specific validated ENCOM bank.
    # The first object in it is DATA id=21; using this anchor narrows probes.
    needle=b'\x40\x00\x00\x00\x00\x00\x00\x00\x15\x00\x00\x00'
    matches=[];errors=[];start=0
    first=d.find(b'UFS2.B.P00000003')
    while True:
        off=d.find(needle,start)
        if off<0:break
        start=off+1
        if not (off<first<off+20000):continue
        try: matches.append(check_at(d,off))
        except (ValueError,struct.error,IndexError) as e:errors.append((off,str(e)))
    if len(matches)!=1:
        raise SystemExit(f'FAIL: expected one verified ENCOM bank, found {len(matches)}; rejected={errors[:3]}')
    r=matches[0]
    print('REDSHRT2 SHA1: PASS | Uplink SAV62: PASS')
    print('ENCOM NATIVE DATABANK: PASS | all six UFS2 boundary records: PASS')
    print(f'Native bank: {SIZE} Gq | file slots: {r["slots"]} | file records: {len(r["files"])} | used: {r["used"]} Gq')
    for label,(lo,hi) in {'ROOT':(0,ROOT),'NODE3':(91,191),'NODE24':(193,293),'NODE29':(295,395)}.items():
        entries=r['regions'][label]
        used=sum(file['size'] for _,file in entries)
        print(f'{label}: range=[{lo},{hi}) files={len(entries)} used={used} free={hi-lo-used} Gq')
        for start,entry in entries:
            print(f'  [{start},{start+entry["size"]}) {entry["title"]}')
    print('ALL DATA REFERENCES: PASS | ALL EXTENTS CONTIGUOUS | NO CROSS-VOLUME FILES')
    print('NOTE: This validates the saved structure, not in-game FileCopier behavior.')
if __name__=='__main__':main()
