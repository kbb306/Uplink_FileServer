/* Verify native Mac DArray mutation is in-place, bounded and roll-backable.
 * Construct exact 0x128 DataBank, 0x268 LAN systems, and 6 UFS2 markers.
 * Deliberately keep original pointer storage allocated for entire test.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/safe_delete.h"
#include "../src/ufs_compact_native.h"
static void i32(unsigned char *a,int off,int v){memcpy(a+off,&v,4);}
static void u64(unsigned char *a,int off,unsigned long v){memcpy(a+off,&v,8);}
static unsigned long rd64(const unsigned char *a,int off){unsigned long v;memcpy(&v,a+off,8);return v;}
struct ctx {
    unsigned char *comp,*file[8],*sys[34],*files_valid,*mem_valid,*sys_valid;
    unsigned long *files,*systems;
    int *memory;
};
static void init(struct ctx *c){
    unsigned char *bank;
    const char *names[]={"UFS2.B.P00000003","UFS2.E.P00000003",
        "UFS2.B.P00000018","UFS2.E.P00000018",
        "UFS2.B.P0000001D","UFS2.E.P0000001D"};
    const int pos[]={90,191,192,293,294,395};
    const int nodes[]={3,16,24,29,32};
    const int screens[]={103,116,124,129,132};
    const int modes[]={1,-1,1,1,-1};
    int i;
    memset(c,0,sizeof(*c));
    c->comp=(unsigned char*)calloc(1,0x2c8);
    c->files=(unsigned long*)calloc(64,sizeof(unsigned long));
    c->files_valid=(unsigned char*)calloc(64,1);
    c->memory=(int*)calloc(396,sizeof(int));
    c->mem_valid=(unsigned char*)calloc(396,1);
    c->systems=(unsigned long*)calloc(34,sizeof(unsigned long));
    c->sys_valid=(unsigned char*)calloc(34,1);
    assert(c->comp&&c->files&&c->files_valid&&c->memory&&c->mem_valid&&c->systems&&c->sys_valid);
    i32(c->comp,0x20,64);
    strcpy((char*)c->comp+0x24,"Encom R&D Network");
    strcpy((char*)c->comp+0x64,"Encom");
    strcpy((char*)c->comp+0xa4,"202.520.10.089");
    bank=c->comp+0x128;
    u64(bank,0x18,64);u64(bank,0x48,396);
    u64(bank,0x28,(unsigned long)c->files);
    u64(bank,0x30,(unsigned long)c->files_valid);
    u64(bank,0x58,(unsigned long)c->memory);
    u64(bank,0x60,(unsigned long)c->mem_valid);
    u64(c->comp,0x268+0x10,34);
    u64(c->comp,0x268+0x20,(unsigned long)c->systems);
    u64(c->comp,0x268+0x28,(unsigned long)c->sys_valid);
    c->file[0]=(unsigned char*)calloc(1,0x60);
    strcpy((char*)c->file[0]+8,"ROOT FILE");
    i32(c->file[0],0x48,2);i32(c->file[0],0x4c,6);
    c->files[0]=(unsigned long)c->file[0];c->files_valid[0]=1;
    for(i=0;i<6;i++){c->memory[i]=0;c->mem_valid[i]=1;}
    for(i=0;i<6;i++){
        c->file[i+1]=(unsigned char*)calloc(1,0x60);
        strcpy((char*)c->file[i+1]+8,names[i]);
        i32(c->file[i+1],0x48,1);i32(c->file[i+1],0x4c,1);
        c->files[i+1]=(unsigned long)c->file[i+1];c->files_valid[i+1]=1;
        c->memory[pos[i]]=i+1;c->mem_valid[pos[i]]=1;
    }
    c->file[7]=(unsigned char*)calloc(1,0x60);
    strcpy((char*)c->file[7]+8,"Monitor_Bypass");
    i32(c->file[7],0x48,2);i32(c->file[7],0x4c,2);
    c->files[7]=(unsigned long)c->file[7];c->files_valid[7]=1;
    for(i=91;i<93;i++){c->memory[i]=7;c->mem_valid[i]=1;}
    u64(bank,0x20,8);u64(bank,0x50,14);
    for(i=0;i<5;i++){
        int n=nodes[i];
        c->sys[n]=(unsigned char*)calloc(1,0x70);
        i32(c->sys[n],0x08,n==32?5:7);
        i32(c->sys[n],0x20,screens[i]-4);
        i32(c->sys[n],0x24,n==32?-1:screens[i]);
        i32(c->sys[n],0x28,modes[i]);
        c->systems[n]=(unsigned long)c->sys[n];c->sys_valid[n]=1;
    }
}
static void cleanup(struct ctx *c){
    int i;
    for(i=0;i<8;i++)free(c->file[i]);
    for(i=0;i<34;i++)free(c->sys[i]);
    free(c->comp);free(c->files);free(c->files_valid);free(c->memory);
    free(c->mem_valid);free(c->systems);free(c->sys_valid);
}
int main(void){
    struct ctx c;
    struct ufs_browser_view v, root, ordinary_root;
    struct ufs_browser_row row, forged;
    struct ufs_safe_delete_report report;
    unsigned char *bank;
    int i;
    const int positions[]={90,191,192,293,294,395};
    init(&c);bank=c.comp+0x128;
    assert(ufs_browser_view_for_screen(c.comp,103,&v)==1 && v.mode=='P');
    assert(ufs_browser_resolve_row(c.comp,&v,0,&row)==1 && row.is_file && row.first_gq==91);
    assert(ufs_browser_view_for_screen(c.comp,116,&root)==1 && root.mode=='R');
    assert(ufs_safe_delete_file(c.comp,&root,&row,&report)==-2);
    assert(ufs_safe_delete_file(c.comp,&v,&(struct ufs_browser_row){.native_row=2,
           .first_gq=90,.length_gq=1,.is_file=1},&report)<0); /* marker */
    forged=row;forged.length_gq=3;
    assert(ufs_safe_delete_file(c.comp,&v,&forged,&report)<0);
    assert(c.files_valid[7]==1 && rd64(bank,0x20)==8 && rd64(bank,0x50)==14);
    /* A stale/out-of-disk file mapping cannot be used. */
    forged=row;forged.first_gq=194;
    assert(ufs_safe_delete_file(c.comp,&v,&forged,&report)<0);
    /* Corrupted marker => fail closed, absolutely no state mutation. */
    c.mem_valid[191]=0;
    assert(ufs_safe_delete_file(c.comp,&v,&row,&report)<0);
    assert(rd64(bank,0x20)==8 && c.files_valid[7] && rd64(bank,0x50)==14);
    c.mem_valid[191]=1;
    /* Successful safe in-place deletion, pointer deliberately NOT freed. */
    assert(ufs_safe_delete_file(c.comp,&v,&row,&report)==0);
    assert(report.file_slot==7 && report.first_gq==91 && report.length_gq==2);
    assert(report.files_before==8&&report.files_after==7&&report.used_before==14&&report.used_after==12);
    assert(!c.files_valid[7] && c.files[7]==0 && rd64(bank,0x20)==7 && rd64(bank,0x50)==12);
    assert(!c.mem_valid[91] && !c.mem_valid[92]);
    for(i=0;i<6;i++){
        assert(c.mem_valid[positions[i]]==1 &&
               c.memory[positions[i]]==i+1 && c.files_valid[i+1]);
    }
    for(i=0;i<6;i++)assert(c.mem_valid[i] && c.memory[i]==0);
    assert(ufs_browser_view_for_screen(c.comp,103,&v)==1);
    assert(ufs_browser_resolve_row(c.comp,&v,0,&row)==1 && !row.is_file &&
           row.first_gq==91 && row.length_gq==100 && row.visible_count==1);
    assert(ufs_browser_view_for_screen(c.comp,124,&v)==1);
    assert(ufs_browser_view_for_screen(c.comp,129,&v)==1);
    assert(ufs_browser_view_for_screen(c.comp,116,&v)==1 && v.mode=='R');
    /* Cannot double-delete using old row. */
    assert(ufs_safe_delete_file(c.comp,&v,&row,&report)<0);
    /* Model a normal shared-root FILESERVER holding TWO ordinary files.
     * A successful native-timed deletion must compact only the shared root. */
    free(c.file[7]);
    c.file[7]=(unsigned char*)calloc(1,0x60);
    assert(c.file[7]);
    strcpy((char*)c.file[7]+8,"ROOT_SECOND");
    i32(c.file[7],0x48,2);i32(c.file[7],0x4c,4);
    c.files[7]=(unsigned long)c.file[7];c.files_valid[7]=1;
    for(i=15;i<19;i++){c.memory[i]=7;c.mem_valid[i]=1;}
    u64(bank,0x20,8);u64(bank,0x50,16);
    assert(ufs_browser_view_for_screen(c.comp,116,&root)==1 && root.mode=='R');
    assert(ufs_browser_resolve_row(c.comp,&root,0,&row)==1 && row.is_file && row.first_gq==0);
    assert(ufs_safe_delete_file(c.comp,&root,&row,&report)==0);
    assert(report.file_slot==0 && report.length_gq==6);
    assert(!c.files_valid[0] && rd64(bank,0x20)==7 && rd64(bank,0x50)==10);
    for(i=0;i<6;i++) assert(!c.mem_valid[i]);
    for(i=0;i<6;i++) assert(c.mem_valid[positions[i]] && c.memory[positions[i]]==i+1);
    assert(ufs_browser_view_for_screen(c.comp,103,&v)==1);
    assert(ufs_browser_view_for_screen(c.comp,116,&root)==1);
    /* Mac's vanilla remote auto-repack, but bounded to the root disk. */
    {
        struct ufs_compact_result packed;
        assert(ufs_compact_native(c.comp,&root,&packed)==0);
        assert(packed.moved_files==1 && packed.files==1 && packed.used_gq==4);
        for(i=0;i<4;i++) assert(c.mem_valid[i] && c.memory[i]==7);
        for(i=4;i<90;i++) assert(!c.mem_valid[i]);
        for(i=0;i<6;i++) assert(c.mem_valid[positions[i]] && c.memory[positions[i]]==i+1);
        for(i=91;i<395;i++)
            if(i!=191&&i!=192&&i!=293&&i!=294) assert(!c.mem_valid[i]);
        assert(rd64(bank,0x20)==7 && rd64(bank,0x50)==10);
        assert(ufs_browser_view_for_screen(c.comp,116,&root)==1);
        assert(ufs_browser_resolve_row(c.comp,&root,0,&row)==1 &&
               row.is_file && row.first_gq==0 && row.length_gq==4);
    }
    /* ENCOM's unmodified type-5 Main Server uses this same shared root.
     * Protect the private disks without disabling normal native file tools. */
    assert(ufs_browser_view_for_screen(c.comp,132,&ordinary_root)==1);
    assert(ordinary_root.mode=='R' && ordinary_root.node_index==32);
    assert(ufs_browser_resolve_row(c.comp,&ordinary_root,0,&row)==1 &&
           row.is_file && row.first_gq==0 && row.length_gq==4);
    assert(ufs_safe_delete_file(c.comp,&ordinary_root,&row,&report)==0);
    assert(report.file_slot==7 && report.length_gq==4);
    assert(rd64(bank,0x20)==6 && rd64(bank,0x50)==6);
    {
        struct ufs_compact_result packed;
        assert(ufs_compact_native(c.comp,&ordinary_root,&packed)==0);
        assert(packed.moved_files==0 && packed.files==0);
    }
    for(i=0;i<90;i++)assert(!c.mem_valid[i]);
    for(i=0;i<6;i++)assert(c.mem_valid[positions[i]] && c.memory[positions[i]]==i+1);
    assert(ufs_browser_view_for_screen(c.comp,103,&v)==1);
    cleanup(&c);
    puts("test_safe_delete: PASS private/root/main-server deletion + root auto-repack, six fixed markers, stale-row rejection");
    return 0;
}
