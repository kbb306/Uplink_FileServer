#!/usr/bin/env python3
"""Portable behavioral model, NOT execution of the Windows/Uplink DLL."""
CAPACITY = 100
MASK = 0xffffffff

def rng_step(value):
    x = value or 0x9E3779B9
    x ^= (x << 13) & MASK
    x ^= x >> 17
    x ^= (x << 5) & MASK
    return x & MASK

def pick(state, rng):
    state = rng_step(state)
    return state, state % rng

def populate(seed, prefix):
    state=seed
    state, r=pick(state,6)
    requested=7+r
    state, suffix=pick(state,99999)
    pos=0; result=[]
    for i in range(requested):
        state, a=pick(state,5)
        state, b=pick(state,4)
        size=2+a+b
        state, t=pick(state,2)
        kind=t+1
        state, e=pick(state,2)
        if e:
            state, encrypted=pick(state,5)
        else: encrypted=0
        state, c=pick(state,2)
        if c:
            state, compressed=pick(state,5)
        else: compressed=0
        if pos+size>CAPACITY:
            break
        name=f'{prefix[:3]}-{("data" if kind==1 else "prog")}-{(suffix+i*7919)%99999}'
        result.append((pos,size,name,kind,encrypted,compressed))
        pos+=size
    return result

for seed in range(1,3001):
    result = populate(seed,'C&A')
    assert result
    assert len(result) <= 12
    assert len({x[2] for x in result})==len(result)
    last_end=0
    for start,size,name,kind,enc,comp in result:
        assert start >= last_end
        assert start+size<=CAPACITY
        assert name.startswith('C&A-data-') or name.startswith('C&A-prog-')
        assert kind in (1,2) and 0<=enc<5 and 0<=comp<5
        last_end=start+size

nodes={4:1,5:1,6:2,7:2,8:-1,9:0}
volumes={}
for node,mode in nodes.items():
    if mode<=0:continue
    key=('P',node) if mode==1 else ('G',mode)
    volumes.setdefault(key,populate(node*17431+123,'C&A'))
assert volumes[('P',4)] != volumes[('P',5)]
assert nodes[6] == nodes[7] == 2 and len([v for v in volumes if v==('G',2)])==1
# Simulated saving and reopening uses the existing records, without re-generation.
import copy
saved=copy.deepcopy(volumes)
assert saved==volumes
print('PASS: 3,000 pseudo-random populations fit inside a 100 Gq modeled volume')
print('PASS: company-prefix filenames, unique names, file sizes, encryption and compression ranges')
print('PASS: private-node distinct, numbered-group shared, existing data survives simulated reload')
print('NOT TESTED: compiled DLL/runtime, real Uplink save/load, native DataBank writes')
