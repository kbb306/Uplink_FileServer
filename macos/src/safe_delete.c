#include "safe_delete.h"
#include "active_layout.h"

#define CP __builtin_memcpy
#define SET __builtin_memset
static unsigned long p64(const unsigned char *p,int at){unsigned long x;CP(&x,p+at,8);return x;}
static int i32(const unsigned char *p,int at){int x;CP(&x,p+at,4);return x;}
static void w64(unsigned char *p,int at,unsigned long x){CP(p+at,&x,8);}
static int sane(unsigned long p){return p>0x10000ul && (p&7ul)==0;}
/* Two markers are occupied per 100-Gq partition, so a private file can
 * never exceed 100. This bounded journal permits exact rollback with no heap. */
int ufs_safe_delete_file(void *computer,
                         const struct ufs_browser_view *view,
                         const struct ufs_browser_row *row,
                         struct ufs_safe_delete_report *out){
    unsigned char *bank,*fv,*mv,*data;
    unsigned long *files;
    int *memory;
    unsigned long old_files_used,old_memory_used,old_ptr;
    unsigned char old_fv,valid[100];
    int slots[100];
    int fs,ms,at,len,slot,i,code,found=0;
    struct ufs_browser_view fresh;
    struct ufs_browser_row fresh_row;
    if(!computer||!view||!row||!out)return -1;
    SET(out,0,sizeof(*out));
    /* The browser revalidates all UFS2 boundaries and node ownership.
     * A grouped FILESERVER is the same physical partition for every member.
     * Journal remains bounded to <=100 Gq per selected file. */
    if(!(((view->mode=='P'||view->mode=='G') &&
          view->end_gq-view->begin_gq==UFS_PRIVATE_CAPACITY) ||
         (view->mode=='R' && view->begin_gq==0 &&
          view->end_gq==view->root_gq && view->root_gq<=UFS_NATIVE_LIMIT_SLOTS)) ||
       !row->is_file ||
       row->first_gq<view->begin_gq || row->length_gq<1 || row->length_gq>100 ||
       row->first_gq>view->end_gq-row->length_gq ||
       row->native_row<0)return -2;
    code=ufs_browser_view_for_screen(computer,view->screen_index,&fresh);
    if(code!=1||fresh.mode!=view->mode||fresh.node_index!=view->node_index||
       fresh.begin_gq!=view->begin_gq||fresh.end_gq!=view->end_gq||
       fresh.bank_gq!=view->bank_gq||fresh.marker_count!=view->marker_count)return -3;
    /* Re-enumerate exact row to detect a stale screen selection. */
    for(i=0;i<20000;i++){
        code=ufs_browser_resolve_row(computer,&fresh,i,&fresh_row);
        if(code<0)return -4;
        if(!code)break;
        if(fresh_row.native_row==row->native_row &&
           fresh_row.first_gq==row->first_gq &&
           fresh_row.length_gq==row->length_gq && fresh_row.is_file){found=1;break;}
    }
    if(!found)return -5;
    bank=(unsigned char*)computer+UFS_COMPUTER_BANK_OFFSET;
    fs=i32(bank,0x18);ms=i32(bank,0x48);
    if(fs<1||fs>UFS_NATIVE_LIMIT_FILES||ms!=view->bank_gq||
       ms<1||ms>UFS_NATIVE_LIMIT_SLOTS)return -6;
    files=(unsigned long*)p64(bank,0x28);
    fv=(unsigned char*)p64(bank,0x30);
    memory=(int*)p64(bank,0x58);
    mv=(unsigned char*)p64(bank,0x60);
    if(!sane((unsigned long)files)||!sane((unsigned long)fv)||
       !sane((unsigned long)memory)||!sane((unsigned long)mv))return -7;
    at=row->first_gq;len=row->length_gq;
    if(!mv[at])return -8;
    slot=memory[at];
    if(slot<0||slot>=fs||!fv[slot]||!sane(files[slot]))return -9;
    data=(unsigned char*)files[slot];
    if(i32(data,UFS_MAC_DATA_SIZE_OFFSET)!=len ||
       (data[8]=='U'&&data[9]=='F'&&data[10]=='S'&&data[11]=='2'&&data[12]=='.'))return -10;
    for(i=0;i<len;i++){
        int pos=at+i;
        if(!mv[pos]||memory[pos]!=slot)return -11;
    }
    /* Verify exact extent and that no reference to this slot lies elsewhere. */
    for(i=0;i<ms;i++)if(mv[i]&&memory[i]==slot && (i<at||i>=at+len))return -12;
    old_files_used=p64(bank,0x20);
    old_memory_used=p64(bank,0x50);
    if(old_files_used<7 || old_files_used>(unsigned long)fs ||
       old_memory_used<(unsigned long)(len+6)||old_memory_used>(unsigned long)ms)return -13;
    old_ptr=files[slot];old_fv=fv[slot];
    for(i=0;i<len;i++){valid[i]=mv[at+i];slots[i]=memory[at+i];}
    /* No native DataBank::RemoveData or DeleteDArrayData. The original
     * pointer is intentionally retained (not freed): it is at most one 0x60
     * byte Data instance per safe delete, until Uplink exits. Since fv=0,
     * native save serialisation ignores that slot and doesn't duplicate it. */
    fv[slot]=0;
    files[slot]=0;
    w64(bank,0x20,old_files_used-1);
    for(i=0;i<len;i++){mv[at+i]=0;memory[at+i]=0;}
    w64(bank,0x50,old_memory_used-(unsigned long)len);
    code=ufs_browser_view_for_screen(computer,view->screen_index,&fresh);
    if(code!=1 || fresh.total_files!=(int)old_files_used-1 ||
       fresh.marker_count!=view->marker_count || fresh.mode!=view->mode||fresh.node_index!=view->node_index){
        /* Restore every modified byte and counter on any failed postcheck. */
        fv[slot]=old_fv;files[slot]=old_ptr;w64(bank,0x20,old_files_used);
        for(i=0;i<len;i++){mv[at+i]=valid[i];memory[at+i]=slots[i];}
        w64(bank,0x50,old_memory_used);
        return -90;
    }
    out->file_slot=slot;out->first_gq=at;out->length_gq=len;
    out->files_before=(int)old_files_used;out->files_after=(int)old_files_used-1;
    out->used_before=(int)old_memory_used;out->used_after=(int)old_memory_used-len;
    return 0;
}
