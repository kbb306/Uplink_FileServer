#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../src/partition_commit.h"

struct mock {
    unsigned char computer[0x2c8];
    uintptr_t files[128];
    unsigned char file_valid[128];
    int memory[512];
    unsigned char memory_valid[512];
    unsigned char data[128][0x60];
    int next_file, next_marker, corrupt_resize, reject_alloc;
};
static struct mock *active;
static void wptr(unsigned char *p,int off,uintptr_t v){memcpy(p+off,&v,sizeof(v));}
static void wint(unsigned char *p,int off,int v){memcpy(p+off,&v,sizeof(v));}
static int rint(const unsigned char *p,int off){int v;memcpy(&v,p+off,4);return v;}
static void init(struct mock *m,int size){
    unsigned char *bank;
    memset(m,0,sizeof(*m));active=m;bank=m->computer+UFS_COMPUTER_BANK_OFFSET;
    strcpy((char*)m->computer+0x24,"Encom R&D Network");
    strcpy((char*)m->computer+0x64,"Encom");
    strcpy((char*)m->computer+0xa4,"202.520.10.089");
    wint(bank,0x18,0);wint(bank,0x48,size);
    wptr(bank,0x28,(uintptr_t)m->files);
    wptr(bank,0x30,(uintptr_t)m->file_valid);
    wptr(bank,0x58,(uintptr_t)m->memory);
    wptr(bank,0x60,(uintptr_t)m->memory_valid);
}
static void file_existing(struct mock *m,int size,int at){
    unsigned char *f=m->data[m->next_file];
    int i,idx=m->next_file++;
    snprintf((char*)f+8,64,"Original file %d",idx);
    wint(f,UFS_MAC_DATA_TYPE_OFFSET,1);
    wint(f,UFS_MAC_DATA_SIZE_OFFSET,size);
    m->files[idx]=(uintptr_t)f;m->file_valid[idx]=1;
    for(i=0;i<size;i++){m->memory_valid[at+i]=1;m->memory[at+i]=idx;}
    wint(m->computer+UFS_COMPUTER_BANK_OFFSET,0x18,m->next_file);
}
static void make_plan(struct ufs_volume_plan *p){
    int slot;
    ufs_plan_reset(p,1);
    assert(ufs_plan_register(p,3,1,&slot)==1 && slot==0);
    assert(ufs_plan_register(p,16,-1,&slot)==0 && slot==-1);
    assert(ufs_plan_register(p,24,1,&slot)==1 && slot==1);
    assert(ufs_plan_register(p,29,1,&slot)==1 && slot==2);
}
static void setsize(void *bank,int n){
    int old=rint(bank,0x48);
    assert(n>old&&n<512);
    wint(bank,0x48,n);
    if(active->corrupt_resize)active->memory[0]=999;
}
static void put_at(void *bank,void *file,int at){
    int idx=active->next_file++;
    assert(at>=0&&at<rint(bank,0x48));
    assert(active->memory_valid[at]==0);
    active->files[idx]=(uintptr_t)file;
    active->file_valid[idx]=1;
    active->memory[at]=idx;active->memory_valid[at]=1;
    wint(bank,0x18,active->next_file);
}
static void *new_data(unsigned long n){
    assert(n==0x60);
    if(active->reject_alloc)return 0;
    assert(active->next_marker<20);
    return (void*)active->data[64+active->next_marker++];
}
static void ctor(void *p){memset(p,0,0x60);}
static void title(void *p,const char *name){strncpy((char*)p+8,name,63);}
static const struct ufs_native_writes natives={setsize,put_at,new_data,ctor,title};
static void test_success_and_no_duplicate(void){
    struct mock m;
    struct ufs_volume_plan p;
    struct ufs_commit_report c;
    struct ufs_bank_report r;
    init(&m,100);make_plan(&p);
    file_existing(&m,3,0);file_existing(&m,4,17);
    assert(ufs_plan_is_valid(&p));
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==0);
    assert(c.stage==2&&c.root_intact==1&&c.before_gq==100&&c.after_gq==406);
    assert(c.created_markers==6&&c.before_files==2&&c.after_files==8);
    assert(c.before_used==7&&c.after_used==13);
    assert(m.memory[0]==0&&m.memory[17]==1&&m.memory_valid[18]);
    assert(ufs_bank_inspect(m.computer,&p,&r)==0&&r.layout_state==1);
    assert(ufs_verify_fresh_layout(m.computer,&p,100)==0);
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==1&&c.stage==0);
}
static void test_identity_and_failure_cases(void){
    struct mock m;struct ufs_volume_plan p;struct ufs_commit_report c;
    init(&m,100);make_plan(&p);
    strcpy((char*)m.computer+0x64,"Wrong Company");
    assert(ufs_plan_is_valid(&p));
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==0&&c.root_intact==1);
    init(&m,100);make_plan(&p);p.volumes[0].id.key=999;
    assert(!ufs_plan_is_valid(&p));
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==-1&&c.stage==0);
    init(&m,100);make_plan(&p);active->reject_alloc=1;
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==-3&&c.stage==0);
    init(&m,100);make_plan(&p);file_existing(&m,2,0);active->corrupt_resize=1;
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==-4&&c.stage==1);
    init(&m,100);make_plan(&p);m.computer[UFS_COMPUTER_BANK_OFFSET+0x68]=1;
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==-2&&c.stage==0);
    init(&m,100);make_plan(&p);
    m.memory_valid[0]=1;m.memory[0]=555;
    assert(ufs_commit_partitions(m.computer,&p,&natives,&c)==-2&&c.stage==0);
}
int main(void){
    test_success_and_no_duplicate();test_identity_and_failure_cases();
    puts("PASS: generic UFS2 marker commit, native placement, root preservation, idempotency and fail closed");
    return 0;
}
