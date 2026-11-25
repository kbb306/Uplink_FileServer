#!/usr/bin/env python3
"""Read-only real-save compaction regression: Mac 396-Gq and Windows 428-Gq.

Loads supplied REDSHRT2 files, finds the actual native ENCOM DArray without
hardcoding file slots or capacity, and exercises the SAME C algorithm used by
both live mods against copies of those memory tables. NEVER modifies saves.
"""
import ctypes as ct
import importlib.util
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('saved',root.parent/'tools/verify_saved_ufs2_active.py')
saved=importlib.util.module_from_spec(spec);spec.loader.exec_module(saved)

def find_encom(path):
    d=saved.payload(path)
    marker=d.find(b'UFS2.B.P00000003')
    if marker<0:raise ValueError('ENCOM private-bank marker missing')
    matches=[]
    # Find slot 0: DArray count (varies with save), slot index 0, DATA type 21.
    for pos in range(max(0,marker-12000),marker):
        if d[pos+4:pos+12]!=b'\0\0\0\0\x15\0\0\0':continue
        if not 1<=saved.u32(d,pos)<=4096:continue
        try:
            fs,p,slots=saved.data_array(d,pos)
            if not (pos<marker<p):continue
            mm,q,cap=saved.memory_array(d,p)
            names=[f['title'] for f in fs.values()]
            if sum(n.startswith('UFS2.') for n in names)!=6:continue
            if not (0<=p<q<len(d) and d[q]==0):continue
            matches.append((fs,mm,cap,slots))
        except (ValueError,IndexError,struct.error):pass
    if len(matches)!=1:raise ValueError(f'Expected 1 ENCOM bank, got {len(matches)}')
    return matches[0]

class Result(ct.Structure):
    _fields_=[('start',ct.c_int),('end',ct.c_int),('files',ct.c_int),
              ('used_gq',ct.c_int),('moved_files',ct.c_int),('freed_gaps',ct.c_int)]
SIZE_CALLBACK=ct.CFUNCTYPE(ct.c_int,ct.c_void_p,ct.c_int)
VERIFY_CALLBACK=ct.CFUNCTYPE(ct.c_int,ct.c_void_p)

def test(lib,save):
    fs,mem,n,slots=find_encom(save)
    bgn={};end={};markers={}
    for i,f in fs.items():
        name=f['title']
        if not name.startswith('UFS2.'):continue
        at=[k for k,v in mem.items() if v==i]
        assert len(at)==1,(name,at)
        markers[name]=at[0]
        kind=name[5];node=int(name[8:],16)
        (bgn if kind=='B' else end)[node]=at[0]
    assert set(bgn)==set(end)=={3,24,29}
    regions=[('ROOT',0,min(bgn.values()))]+[(str(i),bgn[i]+1,end[i]) for i in (3,24,29)]
    assert all(bgn[i]+101==end[i] for i in bgn)
    for slot,f in fs.items():
        block=sorted(k for k,v in mem.items() if v==slot)
        assert len(block)==f['size'] and block[-1]-block[0]+1==f['size']
    original=dict(mem)
    ids=(ct.c_int*n)();used=(ct.c_ubyte*n)()
    for k,v in mem.items():ids[k]=v;used[k]=1
    csize=SIZE_CALLBACK(lambda _ctx,slot: fs[slot]['size'] if slot in fs and
                        not fs[slot]['title'].startswith('UFS2.') else -1)
    cverify=VERIFY_CALLBACK(lambda _ctx:1)
    moved=[]
    for label,lo,hi in regions:
        before={k:ids[k] for k in range(n) if used[k]}
        rep=Result()
        rc=lib.ufs_compact_export(n,lo,hi,slots,ids,used,csize,None,cverify,ct.byref(rep))
        assert rc==0,(save,label,rc)
        assert rep.start==lo and rep.end==hi
        newmem={k:ids[k] for k in range(n) if used[k]}
        for k in range(n):
            if k<lo or k>=hi:assert newmem.get(k)==before.get(k),f'Outside-range mutation at {k}'
        for name,k in markers.items():assert newmem.get(k)==mem[k],f'Marker changed: {name}'
        moved.append((label,rep.moved_files,rep.files))
    for slot,f in fs.items():
        locations=sorted(k for k in range(n) if used[k] and ids[k]==slot)
        assert len(locations)==f['size'] and locations[-1]-locations[0]+1==f['size']
    # Must preserve all records, counts, marker addresses and physical capacity.
    assert set(newmem.values()) == set(original.values())
    print(f'PASS {save.name} | {n} Gq | {slots} file slots | ' +
          ' | '.join(f'{label} moved={count} files={files}' for label,count,files in moved))
    return moved

def run(a,b):
    with tempfile.TemporaryDirectory() as td:
        dylib=Path(td)/'ufs_compact_test.so'
        subprocess.run(['clang','-shared','-fPIC','-O2','-std=c11','-Wall','-Wextra','-Werror',
                        str(root/'ufs_compact_export.c'),'-o',str(dylib)],check=True)
        lib=ct.CDLL(str(dylib))
        lib.ufs_compact_export.argtypes=[ct.c_int,ct.c_int,ct.c_int,ct.c_int,
             ct.POINTER(ct.c_int),ct.POINTER(ct.c_ubyte),SIZE_CALLBACK,ct.c_void_p,
             VERIFY_CALLBACK,ct.POINTER(Result)]
        lib.ufs_compact_export.restype=ct.c_int
        mac=test(lib,a);win=test(lib,b)
        assert sum(x[1] for x in win)>=1,'Windows fixture should have a post-delete gap'
        print('REAL-SAVE COMPACTION TESTS PASSED; original save files unchanged')
if __name__=='__main__':
    if len(sys.argv)!=3:sys.exit('Usage: test_saved_bank_compact.py Mac-save.usr Windows-save.usr')
    run(Path(sys.argv[1]),Path(sys.argv[2]))
