#!/usr/bin/env python3
"""Independent, pure model of UFS2 mode/range rules. NOT a Uplink runtime test."""
import json
CAP=100
nodes={4:-1,5:0,6:1,7:1,8:2,9:2,10:3}
vols={}
for node,mode in nodes.items():
    if mode>0:
        key=('P',node) if mode==1 else ('G',mode)
        vols.setdefault(key,None)
root_size=100
pos=root_size
for key in vols:
    vols[key]=(pos+1,pos+CAP+1)
    pos+=CAP+2
assert vols[('P',6)] != vols[('P',7)]
assert vols[('G',2)] == vols[('G',2)]
def interval(node):
    mode=nodes[node]
    return (0,root_size) if mode<=0 else vols[('P',node) if mode==1 else ('G',mode)]
assert interval(4)==interval(5)==(0,100)
assert interval(6)!=interval(7)
assert interval(8)==interval(9)
assert interval(8)!=interval(10)
reserved=set()
for beg,end in vols.values():reserved.update((beg-1,end))
occupied={3:'general.txt',interval(6)[0]:'rnd.txt',interval(8)[0]:'finance.dat'}
def valid(target,size,start,end):
    return start<=target and target+size<=end and not any(i in reserved or i in occupied for i in range(target,target+size))
assert valid(interval(6)[0]+1,12,*interval(6))
assert not valid(interval(6)[1]-1,2,*interval(6))
assert not valid(interval(8)[1]-1,2,*interval(8))
assert not valid(5,96,*interval(5))
assert all(not (lo<=p<hi) for n in [4,5,7,8,9,10] for lo,hi in [interval(n)] for p in [interval(6)[0]])
# Equivalent save payload: native FAT plus Data entries plus FILESERVER Data1.
snapshot=json.loads(json.dumps({'nodes':nodes,'ranges':[list(x) for x in vols.values()], 'files':occupied}))
assert snapshot['files'][str(interval(6)[0])]=='rnd.txt'
assert snapshot['nodes']['8']==2
print('PASS: shared/root, unique private, group shared, distinct groups')
print('PASS: guard collision and range clipping model')
print('PASS: model serialization round trip')
print('NOT TESTED: Uplink runtime hooks, MSVC ABI, actual .usr save/load')
