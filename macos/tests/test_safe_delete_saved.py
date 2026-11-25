#!/usr/bin/env python3
"""Offline SAV62 test: model fixed-address deletion using an existing test save.

Does not modify the supplied save; writes a temporary file, verifies checksum,
non-target file records, marker Gq positions and full DataBank extent structure.
This does NOT prove the live game's UI hook works. No player save in ZIP.
Usage: python3 tests/test_safe_delete_saved.py /path/to/Marbles-Private-Upload.usr
"""
import sys,struct,hashlib,tempfile,importlib.util
from pathlib import Path
here=Path(__file__).resolve().parent

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    return mod
repair=load('repair_helper',here.parent/'tools/repair_encom_after_deletion.py')
verify=load('verify_helper',here.parent/'tools/verify_saved_ufs2_active.py')

def pack_files(files):
    buf=bytearray(struct.pack('<I',64))
    for i in range(64):
        if i not in files:buf+=struct.pack('<i',-1);continue
        name,attrs=files[i]
        title=name.encode('latin1')+b'\0'
        buf+=struct.pack('<iII',i,21,len(title))+title+attrs
    return bytes(buf)

def run(path):
    before=repair.decode(path)
    files,memory_start,mem,memory_end=repair.locate(before)
    orig=pack_files(files)
    off=memory_start-len(orig)
    assert before[off:memory_start]==orig,'Unexpected original file-array encoding'
    targets=[i for i,(name,_) in files.items() if name=='Monitor_Bypass']
    assert len(targets)==1,'Exactly one named disposable test file required'
    slot=targets[0]
    points=repair.slot_locations(mem,slot)
    assert len(points)==1 and points[0]>=91 and points[0]<191,'Target not in NODE3'
    after_files={i:v for i,v in files.items() if i!=slot}
    after_mem={at:fi for at,fi in mem.items() if fi!=slot}
    revised=before[:off]+pack_files(after_files)+repair.pack_memory(after_mem)+before[memory_end:]
    r=verify.check_at(revised,off)
    assert len(r['files'])==len(files)-1 and r['used']==len(mem)-len(points)
    for m,at in repair.MARKERS.items():
        mi=next(i for i,(name,_) in after_files.items() if name==m)
        assert repair.slot_locations(after_mem,mi)==[at]
    assert revised[:off]==before[:off] and revised[-len(before[memory_end:]):]==before[memory_end:]
    encoded=bytes(b^0x80 for b in revised)
    digest=hashlib.sha1(encoded).digest()
    hdr=b'REDSHRT2\0'+b''.join(digest[i:i+4][::-1] for i in range(0,20,4))
    with tempfile.TemporaryDirectory(prefix='ufs10-saved-test-') as tmp:
        p=Path(tmp)/'SYNTHETIC-DELETE.usr';p.write_bytes(hdr+encoded)
        assert verify.payload(p)==revised
    print('PASS: simulated fixed-address NODE3 deletion, original records unchanged, 6 markers fixed, SAV62 checksum valid')

if __name__=='__main__':
    if len(sys.argv)!=2:raise SystemExit('Usage: test_safe_delete_saved.py /path/to/known-good-test.usr')
    run(Path(sys.argv[1]))
