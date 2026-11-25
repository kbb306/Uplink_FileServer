#include "private_population.h"
#include "active_layout.h"

/* No libc dependency, so this cross-compiles to Mach-O objects on Linux. */
#define COPY __builtin_memcpy
#define CLEAR __builtin_memset
static int get_i32(const void *p,unsigned int at){int v;COPY(&v,(const unsigned char*)p+at,4);return v;}
static unsigned long get_ptr(const void *p,unsigned int at){unsigned long v;COPY(&v,(const unsigned char*)p+at,8);return v;}
static void set_i32(void *p,unsigned int at,int v){COPY((unsigned char*)p+at,&v,4);}
static int sane(unsigned long p){return p>=0x10000ul && !(p&7ul);}
static int equal(const char *a,const char *b){while(*a && *b && *a==*b){a++;b++;}return *a==*b;}
static unsigned int random_next(unsigned int *state){
    unsigned int x=*state;
    x^=x<<13;x^=x>>17;x^=x<<5;*state=x;
    return x;
}
/* Original Uplink's NameGenerator uses the company name's first three
 * letters and the `data` / `prog` suffix. The four-digit node prefix in
 * the random seed and the file-index counter avoid same-LAN collisions. */
static void build_name(char title[64],const char *company,int kind,
                       unsigned int number){
    static const char digits[]="0123456789";
    const char *s=kind==1?"data":"prog";
    int i;
    title[0]=company[0];title[1]=company[1];title[2]=company[2];
    title[3]='-';title[4]=s[0];title[5]=s[1];
    title[6]=s[2];title[7]=s[3];
    title[8]='-';
    for(i=0;i<5;i++){
        title[13-i]=digits[number%10u];number/=10u;
    }
    title[14]=0;
}
static int name_taken(const unsigned char *bank,const char *name){
    int n=get_i32(bank,0x18),i;
    const unsigned long *ptrs=(const unsigned long*)get_ptr(bank,0x28);
    const unsigned char *valid=(const unsigned char*)get_ptr(bank,0x30);
    if(n<0||n>UFS_NATIVE_LIMIT_FILES||!sane((unsigned long)ptrs)||
       !sane((unsigned long)valid))return -1;
    for(i=0;i<n;i++)if(valid[i]){
        if(!sane(ptrs[i]))return -1;
        if(equal((const char*)ptrs[i]+UFS_MAC_DATA_TITLE_OFFSET,name))return 1;
    }
    return 0;
}
static int marker_file(const unsigned char *bank,int at,const char *name,
                       unsigned char **out){
    const unsigned char *valid=(const unsigned char*)get_ptr(bank,0x60);
    const int *map=(const int*)get_ptr(bank,0x58);
    const unsigned long *files=(const unsigned long*)get_ptr(bank,0x28);
    const unsigned char *fv=(const unsigned char*)get_ptr(bank,0x30);
    int n=get_i32(bank,0x18),f;
    if(!sane((unsigned long)valid)||!sane((unsigned long)map)||
       !sane((unsigned long)files)||!sane((unsigned long)fv)||
       at<0||at>=get_i32(bank,0x48)||valid[at]!=1)return -1;
    f=map[at];
    if(f<0||f>=n||!fv[f]||!sane(files[f]))return -1;
    if(!equal((const char*)files[f]+UFS_MAC_DATA_TITLE_OFFSET,name))return -1;
    *out=(unsigned char*)files[f];return 0;
}
static int validate_disk(const void *computer, const struct ufs_volume_plan *plan,
                         int node_index,int data1,const struct ufs_volume **disk,
                         struct ufs_bank_report *report){
    unsigned int i;
    struct ufs_volume_key key;
    if(!computer||!plan||!report||
       ufs_mode_to_key(data1,node_index,&key)!=1||
       !ufs_plan_is_valid(plan)||
       ufs_bank_inspect(computer,plan,report)!=0||
       report->layout_state!=1||report->formatted||report->orphan_markers||
       report->ufs_marker_count!=(int)(2*plan->volume_count)||report->first_marker_gq<1||
       plan->root_size_gq!=report->first_marker_gq||
       plan->proposed_native_total_gq!=report->bank_gq||
       ufs_verify_active_layout(computer,plan,report->first_marker_gq)!=0)return -1;
    for(i=0;i<plan->volume_count;i++)
        if(plan->volumes[i].id.mode==key.mode &&
           plan->volumes[i].id.key==key.key){
            *disk=&plan->volumes[i];return 0;
        }
    return -1;
}

int ufs_populate_private(void *computer,const struct ufs_volume_plan *plan,
                         int node_index,int data1,const struct ufs_native_writes *native,
                         struct ufs_private_seed_report *out){
    unsigned char *bank,*start_marker;
    const struct ufs_volume *disk=0;
    struct ufs_bank_report before,after;
    const unsigned char *occupied;
    unsigned int state;
    char name[64],marker_name[17];
    float version;
    int count,target,begin,end,i,slot,initial_used=0;
    if(!out)return -1;
    CLEAR(out,0,sizeof(*out));out->node=node_index;
    if(!native||!native->put_at||!native->new_data||!native->data_ctor||
       !native->set_title||validate_disk(computer,plan,node_index,data1,&disk,&before)<0)return -1;
    bank=(unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    begin=disk->begin_marker+1;end=disk->end_marker;
    out->begin_gq=begin;out->end_gq=end;
    out->before_files=before.file_count;
    ufs_marker_name(marker_name,'B',disk->id);
    if(marker_file(bank,disk->begin_marker,marker_name,&start_marker)!=0)return -2;
    COPY(&version,start_marker+0x58u,4);
    if(version==UFS_PRIVATE_SEEDED_VERSION)return 1;
    if(version!=1.0f)return -3;
    occupied=(const unsigned char*)get_ptr(bank,0x60);
    if(!sane((unsigned long)occupied))return -3;
    for(i=begin;i<end;i++)if(occupied[i])initial_used++;
    /* Count distinct existing private files as a diagnostic only. */
    {
        int prior=-1;
        const int *map=(const int*)get_ptr(bank,0x58);
        if(!sane((unsigned long)map))return -3;
        for(i=begin;i<end;i++)if(occupied[i] && map[i]!=prior){
            out->existing_files++;prior=map[i];
        }else if(!occupied[i])prior=-1;
    }
    /* Deterministic PRNG; does not perturb Uplink's global RNG. */
    state=0xA93D7C2Bu ^ ((unsigned int)disk->id.key*0x9E3779B9u) ^
          ((unsigned int)before.bank_gq<<8) ^ ((unsigned int)begin<<16);
    if(state==0)state=1;
    target=8+(int)(random_next(&state)%4u);
    for(count=0;count<target;count++){
        int kind=1+(int)(random_next(&state)%2u);
        int size=3+(int)(random_next(&state)%6u);
        int encryption=(random_next(&state)%2u)?(int)(random_next(&state)%5u):0;
        int compression=(random_next(&state)%2u)?(int)(random_next(&state)%5u):0;
        unsigned int code=(unsigned int)disk->id.key*1000u+(unsigned int)count;
        void *file;
        int taken,attempt;
        /* Preserve room for user uploads. Never overwrite existing data. */
        if(initial_used+out->gq_added+size>75){out->skipped_no_room++;continue;}
        occupied=(const unsigned char*)get_ptr(bank,0x60);
        if(!sane((unsigned long)occupied))return -4;
        slot=ufs_find_placement(occupied,before.bank_gq,begin,end,size);
        if(slot<0){out->skipped_no_room++;continue;}
        /* The on-screen vanilla-style five digits cannot uniquely encode
         * all 32-bit group IDs. Probe additional numeric suffixes rather
         * than silently leaving a grouped disk empty on a name collision. */
        for(attempt=0;attempt<256;attempt++){
            build_name(name,(const char*)computer+0x64,kind,
                       (code+(unsigned int)attempt)%100000u);
            taken=name_taken(bank,name);
            if(taken<=0)break;
        }
        if(taken<0)return -4;
        if(taken || attempt==256){out->skipped_no_room++;continue;}
        file=native->new_data(0x60u);
        if(!file)return -5;
        native->data_ctor(file);
        native->set_title(file,name);
        set_i32(file,UFS_MAC_DATA_TYPE_OFFSET,kind);
        set_i32(file,UFS_MAC_DATA_SIZE_OFFSET,size);
        set_i32(file,UFS_MAC_DATA_ENCRYPTED_OFFSET,encryption);
        set_i32(file,UFS_MAC_DATA_COMPRESSED_OFFSET,compression);
        native->put_at(bank,file,slot);
        out->files_added++;
        out->gq_added+=size;
        /* A new native DArray allocation may relocate the bank's arrays. */
    }
    if(ufs_bank_inspect(computer,plan,&after)!=0 ||
       after.file_count!=before.file_count+out->files_added ||
       after.used_gq!=before.used_gq+out->gq_added ||
       ufs_verify_active_layout(computer,plan,before.first_marker_gq)!=0)return -6;
    if(marker_file(bank,disk->begin_marker,marker_name,&start_marker)!=0)return -6;
    /* Data::version is serialized by the native Data::Save/Load methods. It
     * is ignored for these (hidden) boundary markers. This prevents a fully
     * emptied disk from being silently repopulated after a later reload. */
    version=UFS_PRIVATE_SEEDED_VERSION;
    COPY(start_marker+0x58u,&version,4);
    out->marker_stamped=1;
    out->after_files=after.file_count;
    return 0;
}
