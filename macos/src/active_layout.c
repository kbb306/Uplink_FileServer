#include "active_layout.h"
#define CP __builtin_memcpy
#define SET __builtin_memset
static int i32(const unsigned char *b,unsigned int off){int v;CP(&v,b+off,4);return v;}
static unsigned long ptr(const unsigned char *b,unsigned int off){unsigned long v;CP(&v,b+off,8);return v;}
static int same(const char *a,const char *b){if(!a||!b)return 0;while(*a&&*b&&*a==*b){a++;b++;}return *a==*b;}
static int sane(unsigned long v){return v>0x10000ul && (v&7ul)==0;}
/* Each region has two occupied one-Gq guards. Never allow a native file to
 * straddle a guard, or a malformed reference to claim bytes in two regions. */
static int region(const struct ufs_volume_plan *p,int root,int pos){
    unsigned int j;
    if(pos<0)return -1;
    if(pos<root)return 0;
    for(j=0;j<p->volume_count;j++){
        if(pos>=p->volumes[j].begin_marker+1 && pos<p->volumes[j].end_marker)return (int)j+1;
        if(pos==p->volumes[j].begin_marker || pos==p->volumes[j].end_marker)return -2;
    }
    return -1;
}
int ufs_verify_active_layout(const void *computer,const struct ufs_volume_plan *plan,int root){
    const unsigned char *bank;
    const unsigned long *files;
    const unsigned char *fv,*mv;
    const int *memory;
    struct ufs_bank_report rep;
    int cap,fs,i,slot;
    int start[UFS_NATIVE_LIMIT_FILES],last[UFS_NATIVE_LIMIT_FILES],count[UFS_NATIVE_LIMIT_FILES];
    int which[UFS_NATIVE_LIMIT_FILES],marker_expected[UFS_NATIVE_LIMIT_FILES];
    unsigned int j;
    if(!computer||!plan||plan->volume_count==0||plan->volume_count>UFS_MAX_VOLUMES||root<1||root>UFS_NATIVE_LIMIT_SLOTS)return -1;
    if(ufs_bank_inspect(computer,plan,&rep)!=0||rep.layout_state!=1||rep.orphan_markers||
       rep.formatted||rep.first_marker_gq!=root||rep.ufs_marker_count!=(int)(2*plan->volume_count)||
       rep.found_volumes!=(int)plan->volume_count)return -2;
    if(plan->root_size_gq!=root || plan->proposed_native_total_gq!=rep.bank_gq)return -3;
    bank=(const unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    cap=i32(bank,0x48);fs=i32(bank,0x18);
    if(cap!=rep.bank_gq||fs<1||fs>UFS_NATIVE_LIMIT_FILES||cap<1||cap>UFS_NATIVE_LIMIT_SLOTS)return -4;
    files=(const unsigned long*)ptr(bank,0x28);
    fv=(const unsigned char*)ptr(bank,0x30);
    memory=(const int*)ptr(bank,0x58);
    mv=(const unsigned char*)ptr(bank,0x60);
    if(!sane((unsigned long)files)||!sane((unsigned long)fv)||
       !sane((unsigned long)memory)||!sane((unsigned long)mv))return -5;
    for(i=0;i<fs;i++){
        start[i]=-1;last[i]=-1;count[i]=0;which[i]=-1;marker_expected[i]=-1;
    }
    for(j=0;j<plan->volume_count;j++){
        for(i=0;i<2;i++){
            int at=i?plan->volumes[j].end_marker:plan->volumes[j].begin_marker;
            const unsigned char *f;
            char name[17];
            if(at<root || at>=cap || !mv[at])return -6;
            slot=memory[at];
            if(slot<0||slot>=fs||!fv[slot]||!sane(files[slot]))return -7;
            f=(const unsigned char*)files[slot];
            ufs_marker_name(name,i?'E':'B',plan->volumes[j].id);
            if(!same((const char*)f+UFS_MAC_DATA_TITLE_OFFSET,name)||
               i32(f,UFS_MAC_DATA_SIZE_OFFSET)!=1||i32(f,UFS_MAC_DATA_TYPE_OFFSET)!=1||
               i32(f,UFS_MAC_DATA_ENCRYPTED_OFFSET)!=0||
               i32(f,UFS_MAC_DATA_COMPRESSED_OFFSET)!=0)return -8;
            if(marker_expected[slot]!=-1)return -9;
            marker_expected[slot]=at;
        }
    }
    for(i=0;i<cap;i++){
        const unsigned char *f;
        int r;
        if(!mv[i]){
            if(region(plan,root,i)==-2)return -10; /* missing boundary */
            continue;
        }
        slot=memory[i];
        if(slot<0||slot>=fs||!fv[slot]||!sane(files[slot]))return -11;
        if(marker_expected[slot]>=0){
            if(i!=marker_expected[slot])return -12;
            r=-2;
        }else{
            f=(const unsigned char*)files[slot];
            if(f[UFS_MAC_DATA_TITLE_OFFSET]=='U'&&f[UFS_MAC_DATA_TITLE_OFFSET+1]=='F'&&
               f[UFS_MAC_DATA_TITLE_OFFSET+2]=='S'&&f[UFS_MAC_DATA_TITLE_OFFSET+3]=='2'&&
               f[UFS_MAC_DATA_TITLE_OFFSET+4]=='.')return -13;
            r=region(plan,root,i);
            if(r<0)return -14;
        }
        if(count[slot]==0){start[slot]=i;which[slot]=r;}
        else if(last[slot]!=i-1||which[slot]!=r)return -15;
        last[slot]=i;count[slot]++;
    }
    for(i=0;i<fs;i++){
        if(fv[i]){
            const unsigned char *f;
            int size;
            if(!sane(files[i]))return -16;
            f=(const unsigned char*)files[i];size=i32(f,UFS_MAC_DATA_SIZE_OFFSET);
            if(size<=0||size>cap||count[i]!=size||start[i]<0)return -17;
            if(marker_expected[i]>=0&&start[i]!=marker_expected[i])return -18;
            if(marker_expected[i]<0&&which[i]<0)return -19;
        }else if(count[i]!=0)return -20;
    }
    return 0;
}
