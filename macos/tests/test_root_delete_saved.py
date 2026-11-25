#!/usr/bin/env python3
"""Read-only regression on an actual Mac SAV62: remove one root file and
run exact bundled partition compactor against a memory-table COPY.

The player's save is never written or modified. This is a native-data-layout
model, NOT proof that Uplink's runtime task/callback will work live.
"""
import ctypes as ct
import sys
import subprocess
import tempfile
from pathlib import Path
from test_saved_bank_compact import find_encom, Result, SIZE_CALLBACK, VERIFY_CALLBACK

ROOT=Path(__file__).resolve().parent

def run(path):
    fs,mem,cap,slots=find_encom(path)
    assert cap==396
    original=dict(mem)
    root=[]
    for slot,f in fs.items():
        if f['title'].startswith('UFS2.'): continue
        addresses=sorted(i for i,s in mem.items() if s==slot)
        assert len(addresses)==f['size']
        if addresses[-1]<90: root.append((addresses[0],slot,f))
    root.sort()
    assert len(root)>=2,'Need at least two original root files for actual relocation'
    _,victim,victim_file=root[0]
    assert victim_file['size']>0
    files_after={slot:f for slot,f in fs.items() if slot!=victim}
    after={i:slot for i,slot in mem.items() if slot!=victim}
    ids=(ct.c_int*cap)();valid=(ct.c_ubyte*cap)()
    for i,slot in after.items():ids[i]=slot;valid[i]=1
    def actual_verifier(_ctx):
        seen={slot:[] for slot in files_after}
        for i in range(cap):
            if valid[i]:
                slot=ids[i]
                if slot not in seen: return 0
                seen[slot].append(i)
        for slot,f in files_after.items():
            positions=seen[slot]
            if len(positions)!=f['size'] or positions[-1]-positions[0]+1!=f['size']:
                return 0
        for i in range(90,cap):
            if (ids[i] if valid[i] else None)!=original.get(i):return 0
        return 1
    csize=SIZE_CALLBACK(lambda _ctx,slot:files_after[slot]['size'] if slot in files_after and
                        not files_after[slot]['title'].startswith('UFS2.') else -1)
    verify=VERIFY_CALLBACK(actual_verifier)
    with tempfile.TemporaryDirectory(prefix='ufs14-real-save-root-') as td:
        obj=Path(td)/'compactor.so'
        subprocess.run(['clang','-shared','-fPIC','-O2','-Wall','-Wextra','-Werror',
                        str(ROOT/'ufs_compact_export.c'),'-o',str(obj)],check=True)
        lib=ct.CDLL(str(obj));lib.ufs_compact_export.restype=ct.c_int
        lib.ufs_compact_export.argtypes=[ct.c_int,ct.c_int,ct.c_int,ct.c_int,
              ct.POINTER(ct.c_int),ct.POINTER(ct.c_ubyte),SIZE_CALLBACK,ct.c_void_p,
              VERIFY_CALLBACK,ct.POINTER(Result)]
        rep=Result();rc=lib.ufs_compact_export(cap,0,90,slots,ids,valid,csize,None,verify,ct.byref(rep))
        assert rc==0,(rc,rep.moved_files)
        assert rep.moved_files>=1 and rep.files==len(root)-1
        assert actual_verifier(None)
        # Same compacted layout after a second pass (idempotent).
        again=Result();assert lib.ufs_compact_export(cap,0,90,slots,ids,valid,csize,None,verify,ct.byref(again))==0
        assert again.moved_files==0
    print('PASS real Mac save (read-only): simulated root deletion of',victim_file['title'],
          f'({victim_file["size"]} Gq); relocated {rep.moved_files} surviving root files;',
          'all 6 markers and all 3 private disks unchanged')
if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('Usage: test_root_delete_saved.py MacAgent.usr')
    run(Path(sys.argv[1]))
