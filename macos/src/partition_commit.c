#include "partition_commit.h"
/* freestanding-compatible builtins: the Linux cross-builder has no macOS SDK. */
typedef unsigned long uintptr_t;
typedef unsigned long long uint64_t;
#define UINT64_C(x) x##ULL
#define memcpy __builtin_memcpy
#define memset __builtin_memset

static int r32(const unsigned char *p, unsigned int off) {
    int v; memcpy(&v,p+off,4); return v;
}
static uintptr_t rptr(const unsigned char *p, unsigned int off) {
    uintptr_t v; memcpy(&v,p+off,sizeof(v));return v;
}
static void w32(unsigned char *p,unsigned int off,int v) {
    memcpy(p+off,&v,4);
}
static int eq(const char *a,const char *b) {
    if(!a||!b)return 0;
    while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;
}
/* Generic topology guard. The calling parser or browser reconstructs the plan
 * from the LAN's native type-7 system DArray; this checks canonical internal
 * shape, not any company name, IP address or fixed node count. */
int ufs_plan_is_valid(const struct ufs_volume_plan *p) {
    unsigned int i,j,members=0;
    if(!p||!p->fileserver_count||!p->volume_count||
       p->volume_count>UFS_MAX_VOLUMES||
       p->root_fileserver_count>p->fileserver_count)return 0;
    for(i=0;i<p->volume_count;i++){
        const struct ufs_volume *v=&p->volumes[i];
        if((v->id.mode!='P'&&v->id.mode!='G')||
           (v->id.mode=='P'&&v->id.key!=(unsigned int)v->first_node)||
           (v->id.mode=='G'&&v->id.key<2)||
           v->first_node<0||v->first_node>UFS_MAX_NODE_INDEX||
           !v->members||v->capacity_gq!=UFS_PRIVATE_CAPACITY)return 0;
        if(v->id.mode=='P'&&v->members!=1)return 0;
        for(j=0;j<i;j++)if(p->volumes[j].id.mode==v->id.mode&&
                              p->volumes[j].id.key==v->id.key)return 0;
        members+=v->members;
    }
    return members+p->root_fileserver_count==p->fileserver_count;
}
/* A 64-bit fingerprint of the *occupied* portion of the original native FAT.
 * Avoid hashing uninitialized bytes in invalid slots. Any moved/deleted root
 * block changes the fingerprint. Existing files are still game-owned. */
static uint64_t root_hash(const void *computer,int root) {
    const unsigned char *bank=(const unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    const unsigned char *valid=(const unsigned char*)rptr(bank,0x60);
    const int *memory=(const int*)rptr(bank,0x58);
    uint64_t h=UINT64_C(14695981039346656037);
    int i;
    if(!valid || !memory || root<0||root>UFS_NATIVE_LIMIT_SLOTS)return 0;
    for(i=0;i<root;i++) {
        unsigned char b[5]={0,0,0,0,0};
        unsigned int j;
        b[0]=valid[i];
        if(b[0])memcpy(b+1,&memory[i],4);
        for(j=0;j<5;j++){h^=b[j];h*=UINT64_C(1099511628211);}
    }
    return h;
}
/* Confirm every boundary marker exists at its *exact* predicted block and
 * that all 100 private blocks between each pair are still empty. There is
 * deliberately no population step in v0.7. */
int ufs_verify_fresh_layout(const void *computer,const struct ufs_volume_plan *p,
                            int expected_root) {
    const unsigned char *bank;
    const int *memory;
    const unsigned char *valid,*files_valid;
    const uintptr_t *files;
    struct ufs_bank_report r;
    unsigned int v;
    int fslots,capacity;
    if(!computer||!p||expected_root<1||
       ufs_bank_inspect(computer,p,&r)!=0||r.layout_state!=1||
       r.first_marker_gq!=expected_root||
       r.found_volumes!=(int)p->volume_count)return -1;
    bank=(const unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    capacity=r32(bank,0x48);fslots=r32(bank,0x18);
    if(capacity!=p->proposed_native_total_gq ||
       fslots<0||fslots>UFS_NATIVE_LIMIT_FILES)return -2;
    memory=(const int*)rptr(bank,0x58);
    valid=(const unsigned char*)rptr(bank,0x60);
    files=(const uintptr_t*)rptr(bank,0x28);
    files_valid=(const unsigned char*)rptr(bank,0x30);
    if(!memory||!valid||!files||!files_valid)return -3;
    for(v=0;v<p->volume_count;v++) {
        const struct ufs_volume *vol=&p->volumes[v];
        int boundaries[2]={vol->begin_marker,vol->end_marker};
        int i,j;
        for(j=0;j<2;j++) {
            int pos=boundaries[j],slot;
            const unsigned char *f;
            char expect[17];
            if(pos<expected_root||pos>=capacity||valid[pos]!=1)return -4;
            slot=memory[pos];
            if(slot<0||slot>=fslots||files_valid[slot]!=1||!files[slot])return -5;
            f=(const unsigned char*)files[slot];
            ufs_marker_name(expect,j?'E':'B',vol->id);
            if(!eq((const char*)(f+UFS_MAC_DATA_TITLE_OFFSET),expect) ||
               r32(f,UFS_MAC_DATA_TYPE_OFFSET)!=1||
               r32(f,UFS_MAC_DATA_SIZE_OFFSET)!=1||
               r32(f,UFS_MAC_DATA_ENCRYPTED_OFFSET)!=0||
               r32(f,UFS_MAC_DATA_COMPRESSED_OFFSET)!=0)return -6;
        }
        for(i=vol->begin_marker+1;i<vol->end_marker;i++)
            if(valid[i])return -7;
    }
    return 0;
}

/* Return values: -1 bad pointers/shape, -2 pre-existing corruption,
 * -3 not enough capacity, -4 memory/layout drift at resize,
 * -5 commit verification failure. Once result->stage!=0, the experimental
 * save MUST be discarded if a failure is reported. */
int ufs_commit_partitions(void *computer,struct ufs_volume_plan *plan,
                          const struct ufs_native_writes *native,
                          struct ufs_commit_report *result) {
    struct ufs_bank_report before,after;
    void *markers[UFS_MAX_VOLUMES*2];
    unsigned char *bank;
    uint64_t original_hash;
    int total,old,inspected,range,i,j;
    if(!result)return -1;
    memset(result,0,sizeof(*result));
    if(!computer||!plan||!ufs_plan_is_valid(plan)||
       !native||!native->set_size||!native->put_at||
       !native->new_data||!native->data_ctor||!native->set_title||
       plan->volume_count==0||plan->volume_count>UFS_MAX_VOLUMES)return -1;
    inspected=ufs_bank_inspect(computer,plan,&before);
    if(inspected!=0||before.layout_state<0||before.formatted||
       before.bank_gq<=0||before.file_slots>UFS_NATIVE_LIMIT_FILES-2*(int)plan->volume_count||
       before.orphan_markers)return -2;
    result->before_gq=before.bank_gq;
    result->before_files=before.file_count;
    result->before_used=before.used_gq;
    result->layout_state=before.layout_state;
    if(before.layout_state==1)return 1;
    if(before.ufs_marker_count)return -2;
    range=ufs_plan_ranges(plan,before.bank_gq);
    if(range!=0)return -3;
    total=plan->proposed_native_total_gq;
    old=before.bank_gq;
    bank=(unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    original_hash=root_hash(computer,old);
    if(!original_hash)return -2;
    /* Prepare all native Data objects BEFORE touching the bank. The game
     * owns these objects after successful PutData; no native C++ deallocation
     * ABI is assumed on the failure path (at most 64*2 tiny leaks). */
    for(i=0;i<(int)plan->volume_count;i++)for(j=0;j<2;j++) {
        void *f;
        char title[17];
        f=native->new_data(0x60u);
        if(!f)return -3;
        native->data_ctor(f);
        ufs_marker_name(title,j?'E':'B',plan->volumes[i].id);
        native->set_title(f,title);
        w32((unsigned char*)f,UFS_MAC_DATA_TYPE_OFFSET,1);
        w32((unsigned char*)f,UFS_MAC_DATA_SIZE_OFFSET,1);
        w32((unsigned char*)f,UFS_MAC_DATA_ENCRYPTED_OFFSET,0);
        w32((unsigned char*)f,UFS_MAC_DATA_COMPRESSED_OFFSET,0);
        markers[i*2+j]=f;
    }
    native->set_size(bank,total);
    result->stage=1;
    if(r32(bank,0x48)!=total || root_hash(computer,old)!=original_hash)return -4;
    /* The freshly appended area must be entirely unused. If not, refuse to
     * place any markers. This specifically protects an existing root bank. */
    {
        const unsigned char *valid=(const unsigned char *)rptr(bank,0x60);
        if(!valid)return -4;
        for(i=old;i<total;i++)if(valid[i])return -4;
    }
    for(i=0;i<(int)plan->volume_count;i++)for(j=0;j<2;j++) {
        const int index=j?plan->volumes[i].end_marker:plan->volumes[i].begin_marker;
        result->stage=2;
        native->put_at(bank,markers[i*2+j],index);
        result->created_markers++;
    }
    if(ufs_verify_fresh_layout(computer,plan,old)!=0 ||
       root_hash(computer,old)!=original_hash ||
       ufs_bank_inspect(computer,plan,&after)!=0 ||
       after.ufs_marker_count!=(int)plan->volume_count*2 ||
       after.file_count!=before.file_count+(int)plan->volume_count*2 ||
       after.used_gq!=before.used_gq+(int)plan->volume_count*2)return -5;
    result->after_gq=after.bank_gq;
    result->after_files=after.file_count;
    result->after_used=after.used_gq;
    result->layout_state=after.layout_state;
    result->root_intact=1;
    return 0;
}
