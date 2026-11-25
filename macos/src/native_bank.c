#include "native_bank.h"
#if !defined(__LP64__)
#error "Mac native DataBank view requires 64-bit pointers"
#endif
static void raw_copy(void *d,const void *s,unsigned long n) {__builtin_memcpy(d,s,n);}
static void raw_clear(void *d,unsigned long n) {__builtin_memset(d,0,n);}
static int raw_equal(const void *a,const void *b,unsigned long n) {
    const unsigned char *x=(const unsigned char*)a,*y=(const unsigned char*)b;
    unsigned long i;for(i=0;i<n;i++)if(x[i]!=y[i])return 0;return 1;
}
static int same_string(const char *a,const char *b) {
    while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;
}

/* C11, but no C++ runtime or Uplink headers are linked into the dylib. */
static unsigned long ptr_at(const unsigned char *p, unsigned int at) {
    unsigned long x=0; raw_copy(&x,p+at,sizeof(x));return x;
}
static int int_at(const unsigned char *p,unsigned int at) {
    int x=0;raw_copy(&x,p+at,sizeof(x));return x;
}
static int sane_ptr(unsigned long p) {return p>0x10000u && p%(sizeof(void*))==0;}
static int bytes(const unsigned char *p,const unsigned char *expected,unsigned int count){
    return raw_equal(p,expected,count);
}
int ufs_bank_machine_signature(const unsigned char *image) {
    static const unsigned char member[]={0x48,0x8d,0xbb,0x28,0x01,0x00,0x00};
    static const unsigned char get_size[]={0x55,0x48,0x89,0xe5,0x8b,0x47,0x48};
    static const unsigned char get_slots[]={0x55,0x48,0x89,0xe5,0x8b,0x47,0x18};
    static const unsigned char mem_index[]={0x55,0x48,0x89,0xe5,0x4c,0x8b,0x47,0x48};
    static const unsigned char rows[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56};
    static const unsigned char data_size_use[]={0x41,0x8b,0x47,0x4c};
    if(!image)return 0;
    return bytes(image+0xe1b16,member,sizeof(member)) &&
           bytes(image+0xe8070,get_size,sizeof(get_size)) &&
           bytes(image+0xe8090,get_slots,sizeof(get_slots)) &&
           bytes(image+0xe91f0,mem_index,sizeof(mem_index)) &&
           bytes(image+0x5ca30,rows,sizeof(rows)) &&
           bytes(image+0x5cab8,data_size_use,sizeof(data_size_use));
}

/* DataBank +0x8 is data DArray, +0x38 is memory DArray.
 * data size +0x18, ptrs +0x28, valid +0x30;
 * memory size +0x48, ints +0x58, valid +0x60.
 */
struct bank_view {
    const unsigned char *data_valid;
    const unsigned char *memory_valid;
    const unsigned long *files;
    const int *memory;
    int fslots, mslots;
};
static int get_file(const struct bank_view *v,int i,const unsigned char **out) {
    unsigned long obj;
    if(i<0||i>=v->fslots||!v->data_valid[i])return 0;
    obj=v->files[i];
    if(!sane_ptr(obj))return -1;
    *out=(const unsigned char*)obj;return 1;
}
static int marker_descriptor(const unsigned char *f,char title[17]) {
    unsigned int i;
    const unsigned char *p=f+UFS_MAC_DATA_TITLE_OFFSET;
    if(!(p[0]=='U'&&p[1]=='F'&&p[2]=='S'&&p[3]=='2'&&p[4]=='.'))return 0;
    if(int_at(f,UFS_MAC_DATA_TYPE_OFFSET)!=1 ||
       int_at(f,UFS_MAC_DATA_SIZE_OFFSET)!=1 ||
       int_at(f,UFS_MAC_DATA_ENCRYPTED_OFFSET)!=0 ||
       int_at(f,UFS_MAC_DATA_COMPRESSED_OFFSET)!=0)return -1;
    if((p[5]!='B' && p[5]!='E') || p[6]!='.' ||
       (p[7]!='P' && p[7]!='G'))return -1;
    for(i=8;i<16;i++)if(!((p[i]>='0'&&p[i]<='9')||
                             (p[i]>='A'&&p[i]<='F')))return -1;
    if(p[16]!=0)return -1;
    for(i=0;i<17;i++)title[i]=(char)p[i];
    return 1;
}
static int first_memory_for(const struct bank_view *v,int file_slot) {
    int i;
    for(i=0;i<v->mslots;i++)
        if(v->memory_valid[i] && v->memory[i]==file_slot)return i;
    return -1;
}
int ufs_bank_inspect(const void *computer,const struct ufs_volume_plan *plan,
                     struct ufs_bank_report *out) {
    const unsigned char *bank;
    struct bank_view v;
    int i, j, layout_ok=1;
    int positions_begin[UFS_MAX_VOLUMES],positions_end[UFS_MAX_VOLUMES];
    if(!computer||!out)return -1;
    raw_clear(out,sizeof(*out));
    out->first_marker_gq=-1;
    bank=(const unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    v.fslots=int_at(bank,0x18);
    v.mslots=int_at(bank,0x48);
    if(v.fslots<0||v.fslots>UFS_NATIVE_LIMIT_FILES||v.mslots<0||
       v.mslots>UFS_NATIVE_LIMIT_SLOTS)return -2;
    out->bank_gq=v.mslots;out->file_slots=v.fslots;
    out->formatted=(int)bank[0x68];
    if(out->formatted>1)return -3;
    v.files=(const unsigned long*)ptr_at(bank,0x28);
    v.data_valid=(const unsigned char*)ptr_at(bank,0x30);
    v.memory=(const int*)ptr_at(bank,0x58);
    v.memory_valid=(const unsigned char*)ptr_at(bank,0x60);
    if((v.fslots && (!sane_ptr((unsigned long)v.files) ||
                     !sane_ptr((unsigned long)v.data_valid))) ||
       (v.mslots && (!sane_ptr((unsigned long)v.memory) ||
                     !sane_ptr((unsigned long)v.memory_valid))))return -4;
    for(i=0;i<UFS_MAX_VOLUMES;i++){
        positions_begin[i]=-1;positions_end[i]=-1;
    }
    for(i=0;i<v.fslots;i++){
        const unsigned char *f=0;
        char name[17];
        int result=get_file(&v,i,&f),mark,pos=0,matched=0;
        if(result<0)return -5;
        if(result==0)continue;
        ++out->file_count;
        mark=marker_descriptor(f,name);
        if(mark<0) {++out->orphan_markers;layout_ok=0;continue;}
        if(mark==0)continue;
        ++out->ufs_marker_count;
        pos=first_memory_for(&v,i);
        if(pos<0){++out->orphan_markers;layout_ok=0;continue;}
        if(out->first_marker_gq<0||pos<out->first_marker_gq)
            out->first_marker_gq=pos;
        if(!plan){++out->orphan_markers;layout_ok=0;continue;}
        for(j=0;j<(int)plan->volume_count;j++){
            char want[17];
            ufs_marker_name(want,'B',plan->volumes[j].id);
            if(same_string(name,want)) {
                if(positions_begin[j]>=0)layout_ok=0;
                positions_begin[j]=pos;matched=1;break;
            }
            ufs_marker_name(want,'E',plan->volumes[j].id);
            if(same_string(name,want)) {
                if(positions_end[j]>=0)layout_ok=0;
                positions_end[j]=pos;matched=1;break;
            }
        }
        if(!matched){++out->orphan_markers;layout_ok=0;}
    }
    for(i=0;i<v.mslots;i++){
        if(v.memory_valid[i]){
            int slot=v.memory[i];
            if(slot<0||slot>=v.fslots||!v.data_valid[slot])return -6;
            ++out->used_gq;
        }
    }
    if(out->ufs_marker_count==0){
        /* A lone malformed UFS2 file is not an empty partition layout. */
        out->layout_state=out->orphan_markers>0?-1:0;
        return 0;
    }
    if(plan){
        int minimum=-1;
        if(out->ufs_marker_count!=(int)plan->volume_count*2)layout_ok=0;
        for(j=0;j<(int)plan->volume_count;j++){
            if(positions_begin[j]<0||positions_end[j]<0||
               positions_end[j]-positions_begin[j]!=UFS_PRIVATE_CAPACITY+1||
               (minimum>=0&&positions_begin[j]<=minimum)) {
                layout_ok=0;continue;
            }
            minimum=positions_end[j];
            ++out->found_volumes;
        }
    }else layout_ok=0;
    out->layout_state=layout_ok?1:-1;
    return 0;
}
