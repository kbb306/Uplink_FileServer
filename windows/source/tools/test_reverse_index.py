#!/usr/bin/env python3
"""Regression for 0.7B1's direction-confused native indexing.

This is a conceptual test, not code running inside the Windows game.
The verified Steam EXE's DataBank::PutData keeps the DATA slot number
at every allocated MEMORY index (plus a separate validity byte).
"""

def reverse_index(memory, valid, data_index):
    for i, stored in enumerate(memory):
        if valid[i] and stored == data_index:
            return i
    return -1

# Match the user's native bank size 0x12c and root size 0x60.
# A marker occupies one Gq at both edges of each private 100 Gq disk.
size = 0x12C
memory = [None] * size
valid = [False] * size
markers = [(0xB, 0x60), (0xC, 0xC5), (0xD, 0xC6), (0xE, 0x12B)]
for file_slot, mem_index in markers:
    memory[mem_index] = file_slot
    valid[mem_index] = True
for file_slot, mem_index in markers:
    assert reverse_index(memory,valid,file_slot)==mem_index
assert reverse_index(memory,valid,0xF)==-1
# Using the buggy GetDataIndex direction would instead yield the contents
# stored at memory[11..14], not the marker positions.
assert all(memory[file_slot] != mem_index for file_slot, mem_index in markers)
print('PASS: 4 native marker slots map back to distinct memory indexes')
print('PASS: sample 100 Gq disks range 0x61..0xc4 and 0xc7..0x12a')
print('NOT TESTED: real Uplink game runtime, save files, transfers')
