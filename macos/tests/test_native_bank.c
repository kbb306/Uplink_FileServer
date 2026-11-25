/* Host-side 64-bit mock of native Mac DataBank and its embedded DArrays. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/native_bank.h"
struct mock {
    unsigned char computer[0x2c8];
    uintptr_t files[32];
    unsigned char file_valid[32];
    int memory[512];
    unsigned char memory_valid[512];
    unsigned char markers[8][0x68];
};
static void assign(void *where,const void *value,size_t n) {memcpy(where,value,n);}
static void set_int(unsigned char *p,unsigned int at,int x){assign(p+at,&x,sizeof(x));}
static void set_ptr(unsigned char *p,unsigned int at,uintptr_t x){assign(p+at,&x,sizeof(x));}
static void init(struct mock *m,int gq){
    unsigned char *bank=m->computer+UFS_COMPUTER_BANK_OFFSET;
    memset(m,0,sizeof(*m));
    set_int(bank,0x18,8);set_int(bank,0x48,gq);
    set_ptr(bank,0x28,(uintptr_t)m->files);
    set_ptr(bank,0x30,(uintptr_t)m->file_valid);
    set_ptr(bank,0x58,(uintptr_t)m->memory);
    set_ptr(bank,0x60,(uintptr_t)m->memory_valid);
}
static void marker(struct mock *m,int file_slot,int mem_index,
                   char boundary,struct ufs_volume_key key){
    unsigned char *f=m->markers[file_slot];char name[17];
    ufs_marker_name(name,boundary,key);
    memcpy(f+UFS_MAC_DATA_TITLE_OFFSET,name,17);
    set_int(f,UFS_MAC_DATA_TYPE_OFFSET,1);
    set_int(f,UFS_MAC_DATA_SIZE_OFFSET,1);
    m->files[file_slot]=(uintptr_t)f;m->file_valid[file_slot]=1;
    m->memory[mem_index]=file_slot;m->memory_valid[mem_index]=1;
}
static void test_empty_bank(void){
    struct mock m;
    struct ufs_bank_report r;
    struct ufs_volume_plan plan;
    int slot;
    init(&m,100);
    ufs_plan_reset(&plan,1);
    assert(ufs_plan_register(&plan,3,1,&slot)==1);
    assert(ufs_plan_register(&plan,16,-1,&slot)==0);
    assert(ufs_plan_register(&plan,24,1,&slot)==1);
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0);
    assert(r.bank_gq==100&&r.file_count==0&&r.used_gq==0&&r.layout_state==0);
}
static void test_valid_boundaries(void){
    struct mock m;
    struct ufs_bank_report r;
    struct ufs_volume_plan plan;
    int slot;
    init(&m,512);
    ufs_plan_reset(&plan,1);
    assert(ufs_plan_register(&plan,3,1,&slot)==1);
    assert(ufs_plan_register(&plan,24,1,&slot)==1);
    marker(&m,0,100,'B',plan.volumes[0].id);
    marker(&m,1,201,'E',plan.volumes[0].id);
    marker(&m,2,202,'B',plan.volumes[1].id);
    marker(&m,3,303,'E',plan.volumes[1].id);
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0);
    assert(r.ufs_marker_count==4&&r.orphan_markers==0);
    assert(r.first_marker_gq==100&&r.used_gq==4);
    assert(r.layout_state==1&&r.found_volumes==2);
}
static void test_corruption_fail_closed(void){
    struct mock m;
    struct ufs_bank_report r;
    struct ufs_volume_plan plan;
    int slot;
    init(&m,400);
    ufs_plan_reset(&plan,1);
    assert(ufs_plan_register(&plan,3,1,&slot)==1);
    marker(&m,0,100,'B',plan.volumes[0].id);
    marker(&m,1,201,'E',plan.volumes[0].id);
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0&&r.layout_state==1);
    /* Broken end guard: data remains intact, but future writes must fail. */
    m.memory_valid[201]=0;
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0&&r.layout_state==-1);
    assert(r.orphan_markers>0);
    m.memory_valid[201]=1;
    /* Corrupted mapping: invalid file slot; observer rejects bank entirely. */
    m.memory[201]=255;
    assert(ufs_bank_inspect(m.computer,&plan,&r)==-6);
    m.memory[201]=1;
    /* Malformed marker field must not be mistaken for an unpartitioned bank. */
    m.markers[1][UFS_MAC_DATA_TITLE_OFFSET+5]='X';
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0&&r.layout_state==-1);
    assert(r.orphan_markers>0);
    /* Malformed-only UFS2 inventory must never be called unpartitioned. */
    m.file_valid[0]=0;m.memory_valid[100]=0;
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0&&r.layout_state==-1);
}
static void test_grouped(void){
    struct mock m;
    struct ufs_bank_report r;
    struct ufs_volume_plan plan;
    int slot;
    init(&m,400);
    ufs_plan_reset(&plan,2);
    assert(ufs_plan_register(&plan,4,2,&slot)==1);
    assert(ufs_plan_register(&plan,8,2,&slot)==1);
    assert(plan.volume_count==1&&plan.volumes[0].members==2);
    marker(&m,0,100,'B',plan.volumes[0].id);
    marker(&m,1,201,'E',plan.volumes[0].id);
    assert(ufs_bank_inspect(m.computer,&plan,&r)==0&&r.layout_state==1);
}
int main(void){
    test_empty_bank();test_valid_boundaries();test_corruption_fail_closed();test_grouped();
    puts("PASS: native 64-bit DataBank inspection, private/group markers, fail-closed integrity");
    return 0;
}
