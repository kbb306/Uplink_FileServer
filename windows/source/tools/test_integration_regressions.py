#!/usr/bin/env python3
"""Static control-flow guard for 0.7C2 integration; not a runtime game test."""
from pathlib import Path
s = (Path(__file__).resolve().parents[1] / 'src/fileserver_fix.c').read_text()
start = s.index('static int resolve_layout(')
end = s.index('static int __cdecl partition_row_scan(', start)
block = s[start:end]
# The working 0.7B2 DArray reverse lookup must be retained (NOT the old wrong
# 0.7C call into 0x00422F10, which retrieves data index from a memory index).
assert 'static int file_slot_to_memory_index(void *bank,int dataindex)' in s
assert 'pos=file_slot_to_memory_index((void*)bank,i);' in s
assert 'RVA_BANK_GETMEMINDEX' not in s
assert 'n_get_mem_index' not in s
# Partition population may only occur after verified native markers and valid layout.
for needle in [
    'if(!initialize_volumes(bank,l))return -15;',
    'found=find_markers(bank,l);',
    'if(found!=expected)return -17;',
    'integrity=valid_layout(bank,l);',
    'if(integrity<0)return integrity-40;',
    'if(newly_created) {',
    '(void)populate_volume(lan,bank,&l->vol[i]);',
    'POPULATE: marker layout intact after native file placement',
]:
    assert needle in block, needle
assert block.index('integrity=valid_layout(bank,l);') < block.index('if(newly_created) {')
assert block.index('if(newly_created) {') < block.index('(void)populate_volume(lan,bank,&l->vol[i]);')
# Existing markers -> newly_created remains false, no regeneration.
assert 'int mode,found,i,expected,integrity,newly_created=0;' in block
assert block.index('newly_created=1;') > block.index('if(found==0 && may_create)')
# Preserve native file operations, not add an independent external disk format.
assert 'n_is_valid((void*)bank,file,pos)' in s
assert 'n_put_at((void*)bank,file,found);' in s
assert '#define PRIVATE_CAPACITY 100' in s
assert 'UFS2' in s
print('PASS: 0.7B2 reverse-index fix retained; wrong forward lookup not resurrected')
print('PASS: population occurs only after marker and layout validation')
print('PASS: saved existing volumes cannot trigger population during normal browsing')
print('PASS: native placement rules and native DataBank writes are used')

# Root-only FILESERVER regression (Mac v0.16.1 required a fix for this).
# A LAN with only DATA1=-1/0 FILESERVERs must use the original native bank,
# without requiring UFS2 partition markers. Corrupt leftover markers must
# still fail closed instead of exposing a potentially inconsistent disk.
assert 'if(!l->num){' in block
assert 'if(f && is_marker(f))return -14;' in block
assert 'l->state=0;return 0;' in block
assert block.index('if(!l->num){') < block.index('found=find_markers(bank,l);')
row_start = s.index('static int __cdecl partition_row_scan(')
row_end = s.index('static int first_native_boundary(', row_start)
row_block = s[row_start:row_end]
assert 'if(l.state==0)return g_original_row_scan(bank,row,data,size,memindex);' in row_block
assert row_block.index('if(layout_result<0)') < row_block.index('if(l.state==0)return g_original_row_scan')
placement_start = s.index('static int __attribute__((thiscall)) partition_find_valid(')
placement_end = s.index('static int install_row_partitions(', placement_start)
placement_block = s[placement_start:placement_end]
assert 'if(resolve_layout(lan,(U32)bank,node,&l,0)<0)return -1;' in placement_block
assert 'if(l.state==1){first=l.active_start;last=l.active_end;}' in placement_block
print('PASS: Windows root-only FILESERVER uses native file rows and placement; stray markers fail closed')

print('NOT TESTED: native runtime, file transfer, save/load on Windows')
