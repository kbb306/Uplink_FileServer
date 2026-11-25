#!/usr/bin/env python3
"""Read-only UFS2 partition verifier for Windows 1.55 / Mac 1.6.9 saves.

Checks REDSHRT2 checksum, SAV62, native data records, physical Gq extents,
and exact UFS2 marker boundaries. Does not modify any file or require Uplink.
"""
import argparse
import hashlib
import struct
from pathlib import Path


def u32(d,p):return struct.unpack_from('<I',d,p)[0]
def i32(d,p):return struct.unpack_from('<i',d,p)[0]
def decode(path):
    b=path.read_bytes()
    if not b.startswith(b'REDSHRT2\0'):raise ValueError('Not a REDSHRT2 .usr save')
    sha=hashlib.sha1(b[29:]).digest()
    if b[9:29]!=b''.join(sha[k:k+4][::-1] for k in range(0,20,4)):
        raise ValueError('Save SHA1 checksum mismatch')
    d=bytes(ch^128 for ch in b[29:])
    if not d.startswith(b'SAV62'):raise ValueError('Unsupported save format')
    return d

def parse_at(d,off):
    n=u32(d,off);p=off+4
    if not 1<=n<=4096:raise ValueError('File slots out of bounds')
    files={}
    for _ in range(n):
        idx=i32(d,p);p+=4
        if idx==-1:continue
        if idx<0 or idx>=n or idx in files:raise ValueError('Invalid DATA index')
        if u32(d,p)!=21:raise ValueError('Not a native DATA object')
        sz=u32(d,p+4);p+=8
        if sz<1 or sz>128 or p+sz+24>len(d) or d[p+sz-1]!=0:
            raise ValueError('Bad DATA name')
        name=d[p:p+sz-1].decode('latin-1');p+=sz
        typ,size,enc,comp,vers,soft=struct.unpack_from('<iiiifi',d,p);p+=24
        if size<1 or size>20000:raise ValueError('Invalid DATA size')
        files[idx]=(name,size,typ,enc,comp)
    cap=u32(d,p);p+=4
    if not 1<=cap<=20000:raise ValueError('Memory capacity out of range')
    memory={}
    for _ in range(cap):
        at=i32(d,p);p+=4
        if at==-1:continue
        if at<0 or at>=cap or at in memory:raise ValueError('Invalid Gq address')
        fi=i32(d,p);p+=4
        if fi not in files:raise ValueError('Orphaned memory reference')
        memory[at]=fi
    if p>=len(d) or d[p]!=0:raise ValueError('Invalid DataBank format flag')
    return files,memory,cap

def inspect(files,memory,cap):
    markers={};members={i:[] for i in files}
    for at,slot in memory.items():members[slot].append(at)
    for slot,(name,size,typ,enc,comp) in files.items():
        places=sorted(members[slot])
        if len(places)!=size or not places or places[-1]-places[0]+1!=len(places):
            raise ValueError(f'Invalid extent: {name}')
        if name.startswith('UFS2.'):
            if name in markers:raise ValueError('Duplicated marker')
            if len(name)!=16 or name[4]!='.' or name[5] not in 'BE' or name[6]!='.' or name[7] not in 'PG':
                raise ValueError('Malformed UFS2 marker')
            if (size,typ,enc,comp)!=(1,1,0,0):raise ValueError('Invalid marker metadata')
            markers[name]=places[0]
    if len(markers)<2 or len(markers)%2:raise ValueError('Missing UFS2 markers')
    groups={}
    for name,pos in markers.items():
        key=name[7:];kind=name[5]
        groups.setdefault(key,{})
        if kind in groups[key]:raise ValueError('Duplicate boundary')
        groups[key][kind]=pos
    if len(groups)*2!=len(markers):raise ValueError('Unpaired markers')
    disks=[]
    for key,g in groups.items():
        if sorted(g)!=['B','E'] or g['E']!=g['B']+101:
            raise ValueError(f'Misaligned/incorrectly sized disk {key}')
        disks.append((g['B'],g['E'],key))
    disks.sort()
    root=disks[0][0]
    for i,(b,e,key) in enumerate(disks):
        if b<1 or e>=cap:raise ValueError('Marker outside bank')
        if i>0 and b!=disks[i-1][1]+1:raise ValueError('Gap/overlap between partitions')
    if disks[-1][1]!=cap-1:raise ValueError('Final marker not last Gq')
    regions=[('ROOT',0,root)]+[(key,b+1,e) for b,e,key in disks]
    totals={label:0 for label,_,_ in regions};counts=dict.fromkeys(totals,0)
    for slot,(name,size,_,_,_) in files.items():
        if name.startswith('UFS2.'):continue
        occupied=sorted(members[slot]);lo=occupied[0];hi=occupied[-1]+1
        match=[label for label,start,end in regions if start<=lo and hi<=end]
        if len(match)!=1:raise ValueError(f'Cross-volume file {name} [{lo},{hi})')
        label=match[0];totals[label]+=size;counts[label]+=1
    print(f'Native bank: {cap} Gq | {len(files)} files | {len(markers)} verified markers')
    for label,lo,hi in regions:
        print(f'  {label:>8} Gq [{lo},{hi}) files={counts[label]} used={totals[label]} free={hi-lo-totals[label]}')
    print('PASS: marker positions, Data extents, region confinement, checksum')
    return cap

def find_bank(d):
    off=0;matches=[]
    # Scan for file arrays starting at native DATA slot zero, then parse only
    # banks with UFS2 marker names. Handles 48-slot Windows and 64-slot Mac.
    pattern=b'\x00\x00\x00\x00\x15\x00\x00\x00'
    while True:
        at=d.find(pattern,off)
        if at<0:break
        off=at+1
        start=at-4
        if start<0 or not 1<=u32(d,start)<=4096:continue
        try:
            files,memory,cap=parse_at(d,start)
            if sum(name.startswith('UFS2.') for name,*_ in files.values())>=2:
                matches.append((start,files,memory,cap))
        except (IndexError,ValueError,struct.error):pass
    return matches

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('save',type=Path)
    args=ap.parse_args()
    d=decode(args.save)
    banks=find_bank(d)
    if not banks:raise SystemExit('FAIL: no native UFS2 DataBank found')
    print(f'PASS REDSHRT2/SAV62; {len(banks)} UFS2 DataBank(s) located')
    for i,(_,files,memory,cap) in enumerate(banks,1):
        print(f'Bank #{i}:')
        inspect(files,memory,cap)
if __name__=='__main__':main()
