/* Use realistic native DataBank DArray layouts, with one 0x60 Data per file.
 * Tests both untouched and previously-uploaded private partitions. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/private_population.h"
#include "../src/active_layout.h"

#define NF 256
#define NM 512
struct mock {
    unsigned char comp[0x2c8];
    uintptr_t file_ptrs[NF];
    unsigned char file_valid[NF];
    int mem[NM];
    unsigned char mem_valid[NM];
    unsigned char data[NF][0x60];
    int allocated,next_slot,fail_alloc;
};
static struct mock *current;
static void wr32(void *p,int off,int v){memcpy((char*)p+off,&v,4);}
static int rd32(const void *p,int off){int v;memcpy(&v,(const char*)p+off,4);return v;}
static void wrptr(void *p,int off,uintptr_t v){memcpy((char*)p+off,&v,8);}
static void init(struct mock *m){
    unsigned char *b;
    memset(m,0,sizeof(*m));current=m;b=m->comp+UFS_COMPUTER_BANK_OFFSET;
    strcpy((char*)m->comp+0x24,"Encom R&D Network");
    strcpy((char*)m->comp+0x64,"Encom");
    strcpy((char*)m->comp+0xa4,"202.520.10.089");
    wr32(b,0x48,90);
    wrptr(b,0x28,(uintptr_t)m->file_ptrs);
    wrptr(b,0x30,(uintptr_t)m->file_valid);
    wrptr(b,0x58,(uintptr_t)m->mem);
    wrptr(b,0x60,(uintptr_t)m->mem_valid);
}
static void set_size(void *b,int n){assert(n>0 && n<=NM);wr32(b,0x48,n);}
static void put_at(void *bank,void *f,int at){
    int k=current->next_slot,sz=rd32(f,UFS_MAC_DATA_SIZE_OFFSET),i;
    assert(k<NF && sz>0 && at>=0 && at+sz<=rd32(bank,0x48));
    for(i=at;i<at+sz;i++)assert(current->mem_valid[i]==0);
    current->file_ptrs[k]=(uintptr_t)f;current->file_valid[k]=1;
    for(i=at;i<at+sz;i++){
        current->mem_valid[i]=1;current->mem[i]=k;
    }
    current->next_slot++;
    wr32(bank,0x18,current->next_slot);
}
static void* new_data(unsigned long n){
    assert(n==0x60);
    if(current->fail_alloc)return NULL;
    assert(current->allocated<NF);
    return current->data[current->allocated++];
}
static void ctor(void *p){float v=1.0f;memset(p,0,0x60);memcpy((char*)p+0x58,&v,4);}
static void title(void *p,const char *name){
    assert(strlen(name)<64);strcpy((char*)p+8,name);
}
static const struct ufs_native_writes api={set_size,put_at,new_data,ctor,title};
static void ordinary_file(const char *name,int pos,int size){
    void *f=new_data(0x60);ctor(f);title(f,name);
    wr32(f,UFS_MAC_DATA_TYPE_OFFSET,1);
    wr32(f,UFS_MAC_DATA_SIZE_OFFSET,size);
    put_at(current->comp+UFS_COMPUTER_BANK_OFFSET,f,pos);
}
static void make_plan(struct ufs_volume_plan *p){
    int slot;
    ufs_plan_reset(p,0);
    assert(ufs_plan_register(p,3,1,&slot)==1);
    assert(ufs_plan_register(p,16,-1,&slot)==0);
    assert(ufs_plan_register(p,24,1,&slot)==1);
    assert(ufs_plan_register(p,29,1,&slot)==1);
}
static void setup(struct mock *m,struct ufs_volume_plan *p){
    struct ufs_commit_report c;
    init(m);make_plan(p);
    ordinary_file("ROOT-keep",0,9);
    ordinary_file("ROOT-keep2",9,5);
    assert(ufs_commit_partitions(m->comp,p,&api,&c)==0);
    assert(c.created_markers==6);
    assert(ufs_verify_active_layout(m->comp,p,90)==0);
}
static void count_volume(struct mock *m,int start,int end,int *files,int *gq){
    int i,old=-1;
    *files=*gq=0;
    for(i=start;i<end;i++){
        if(m->mem_valid[i]){
            ++*gq;
            if(m->mem[i]!=old)(*files)++;
            old=m->mem[i];
        }else old=-1;
    }
}
static void test_all_and_reload(void){
    struct mock m;struct ufs_volume_plan p;
    struct ufs_private_seed_report r;
    int n,g,root0,oldfiles,j,i;
    setup(&m,&p);
    /* An older save may already contain a user upload in private disk 3. */
    ordinary_file("LAN_Force",91,4);
    root0=m.mem[0];oldfiles=m.next_slot;
    for(j=0;j<3;j++){
        int node=j==0?3:j==1?24:29;
        int begin=p.volumes[j].begin_marker+1,end=p.volumes[j].end_marker;
        assert(ufs_populate_private(m.comp,&p,node,1,&api,&r)==0);
        assert(r.node==node && r.files_added>=7 && r.files_added<=11);
        assert(r.marker_stamped==1 && r.after_files==r.before_files+r.files_added);
        assert(r.begin_gq==begin && r.end_gq==end);
        count_volume(&m,begin,end,&n,&g);
        assert(n==r.files_added+(j==0?1:0));
        assert(g==r.gq_added+(j==0?4:0));
        assert(ufs_verify_active_layout(m.comp,&p,90)==0);
        assert(ufs_populate_private(m.comp,&p,node,1,&api,&r)==1);
    }
    assert(m.next_slot>oldfiles && m.mem[0]==root0);
    assert(m.mem_valid[91] && strcmp((char*)m.file_ptrs[m.mem[91]]+8,"LAN_Force")==0);
    assert(ufs_verify_active_layout(m.comp,&p,90)==0);
    /* Delete the whole contents of disk 24 using a simulated native
     * RemoveDataFile (clear its Gq map and file DArray validity entries).
     * The persistent marker must stop regeneration even when truly empty. */
    for(i=193;i<293;i++)if(m.mem_valid[i]){
        int file_slot=m.mem[i];
        m.file_valid[file_slot]=0;
        m.mem_valid[i]=0;
    }
    assert(ufs_verify_active_layout(m.comp,&p,90)==0);
    assert(ufs_populate_private(m.comp,&p,24,1,&api,&r)==1);
    count_volume(&m,193,293,&n,&g);
    assert(n==0 && g==0);
    {
        unsigned char *marker=(unsigned char*)m.file_ptrs[m.mem[192]];
        float v=0;memcpy(&v,marker+0x58,4);
        assert(v==UFS_PRIVATE_SEEDED_VERSION);
    }
}
static void test_full_disk_and_fails_closed(void){
    struct mock m;struct ufs_volume_plan p;
    struct ufs_private_seed_report r;
    setup(&m,&p);
    ordinary_file("occupies-most-disk",91,80);
    assert(ufs_populate_private(m.comp,&p,3,1,&api,&r)==0);
    assert(r.files_added==0 && r.marker_stamped==1);
    assert(ufs_populate_private(m.comp,&p,3,1,&api,&r)==1);
    assert(ufs_verify_active_layout(m.comp,&p,90)==0);
    setup(&m,&p);
    strcpy((char*)m.comp+0x64,"OtherCo");
    assert(ufs_populate_private(m.comp,&p,3,1,&api,&r)==0);
    setup(&m,&p);
    m.mem_valid[90]=0;
    assert(ufs_populate_private(m.comp,&p,3,1,&api,&r)<0);
    setup(&m,&p);
    assert(ufs_populate_private(m.comp,&p,16,-1,&api,&r)<0);
    m.fail_alloc=1;
    assert(ufs_populate_private(m.comp,&p,3,1,&api,&r)<0);
    assert(ufs_verify_active_layout(m.comp,&p,90)==0);
}
int main(void){
    test_all_and_reload();test_full_disk_and_fails_closed();
    puts("PASS: v0.15 private DATA population, pre-existing uploads, marker persistence, no overwrites, bounds and fail-closed checks");
    return 0;
}
