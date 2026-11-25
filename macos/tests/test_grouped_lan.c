/* Full native-object mock: distinct names/IPs, mixed root/private/group nodes.
 * Two FILESERVERs with DATA1=2 must share ONE persisted physical partition. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/private_population.h"
#include "../src/browser_route.h"
#include "../src/safe_delete.h"
#include "../src/ufs_compact_native.h"
#include "../src/active_layout.h"
#define NF 256
#define NM 512
#define NS 40
struct mock {
    unsigned char comp[0x2d0];
    uintptr_t file_ptrs[NF],system_ptrs[NS];
    unsigned char file_valid[NF],memory_valid[NM],system_valid[NS];
    int memory[NM];
    unsigned char file_data[NF][0x60],systems[NS][0x70];
    int next_file,next_object;
};
static struct mock *active;
static void wi(void *p,int off,int x){memcpy((char*)p+off,&x,4);}
static void wp(void *p,int off,uintptr_t x){memcpy((char*)p+off,&x,8);}
static int ri(const void *p,int off){int n;memcpy(&n,(const char*)p+off,4);return n;}
static void *new_data(unsigned long n){assert(n==0x60 && active->next_object<NF);return active->file_data[active->next_object++];}
static void ctor(void *p){float v=1.0f;memset(p,0,0x60);memcpy((char*)p+0x58,&v,4);}
static void title(void *p,const char *name){assert(strlen(name)<64);strcpy((char*)p+8,name);}
static void set_size(void *bank,int n){assert(n>0&&n<=NM);wi(bank,0x48,n);}
static void put_at(void *bank,void *file,int at){
    int slot=active->next_file++,size=ri(file,UFS_MAC_DATA_SIZE_OFFSET),i;
    assert(slot<NF && size>0 && at>=0 && at+size<=ri(bank,0x48));
    for(i=at;i<at+size;i++)assert(!active->memory_valid[i]);
    active->file_ptrs[slot]=(uintptr_t)file;active->file_valid[slot]=1;
    for(i=at;i<at+size;i++){
        active->memory_valid[i]=1;active->memory[i]=slot;
    }
    wi(bank,0x18,active->next_file);
    wp(bank,0x20,(uintptr_t)(active->next_file));
    /* p+0x20 must be the number of live files, not DArray size. */
    wp(bank,0x50,(uintptr_t)0); /* adjusted below by counting occupied Gq */
    {int used=0;for(i=0;i<ri(bank,0x48);i++)if(active->memory_valid[i])used++;
     wp(bank,0x50,(uintptr_t)used);}
}
static const struct ufs_native_writes native={set_size,put_at,new_data,ctor,title};
static void system_at(struct mock *m,int id,int type,int screen,int data1){
    unsigned char *s=m->systems[id];
    wi(s,0x08,type);wi(s,0x20,screen-4);
    wi(s,0x24,type==5?-1:screen);wi(s,0x28,data1);
    m->system_ptrs[id]=(uintptr_t)s;m->system_valid[id]=1;
}
static void setup(struct mock *m,struct ufs_volume_plan *p){
    unsigned char *bank;
    int ix;
    memset(m,0,sizeof(*m));active=m;
    bank=m->comp+UFS_COMPUTER_BANK_OFFSET;
    wi(m->comp,0x20,64);
    strcpy((char*)m->comp+0x24,"Nelvana Foundation");
    strcpy((char*)m->comp+0x64,"Nelvana");
    strcpy((char*)m->comp+0xa4,"123.456.789.012");
    wi(bank,0x48,90);
    wp(bank,0x28,(uintptr_t)m->file_ptrs);
    wp(bank,0x30,(uintptr_t)m->file_valid);
    wp(bank,0x58,(uintptr_t)m->memory);
    wp(bank,0x60,(uintptr_t)m->memory_valid);
    wi(m->comp+0x268,0x10,NS);
    wp(m->comp+0x268,0x20,(uintptr_t)m->system_ptrs);
    wp(m->comp+0x268,0x28,(uintptr_t)m->system_valid);
    system_at(m,3,7,103,1);system_at(m,16,7,116,-1);
    system_at(m,24,7,124,2);system_at(m,29,7,129,2);
    system_at(m,32,5,132,0);
    ufs_plan_reset(p,0);
    assert(ufs_plan_register(p,3,1,&ix)==1 && ix==0);
    assert(ufs_plan_register(p,16,-1,&ix)==0);
    assert(ufs_plan_register(p,24,2,&ix)==1 && ix==1);
    assert(ufs_plan_register(p,29,2,&ix)==1 && ix==1);
    assert(p->volume_count==2 && p->volumes[1].members==2 && ufs_plan_is_valid(p));
    {
        unsigned char *f=new_data(0x60);ctor(f);title(f,"ROOT KEEP");
        wi(f,UFS_MAC_DATA_TYPE_OFFSET,1);wi(f,UFS_MAC_DATA_SIZE_OFFSET,5);
        put_at(bank,f,0);
    }
}
int main(void){
    struct mock m;
    struct ufs_volume_plan p,from_save;
    struct ufs_commit_report cr;
    struct ufs_private_seed_report sr;
    struct ufs_browser_view a,b,priv,root;
    struct ufs_browser_row row;
    struct ufs_safe_delete_report dr;
    struct ufs_compact_result rr;
    int n_before,i;
    setup(&m,&p);
    assert(ufs_plan_matches_native_lan(m.comp,&p));
    wi(m.systems[29],0x28,3);
    assert(!ufs_plan_matches_native_lan(m.comp,&p));
    wi(m.systems[29],0x28,2);
    assert(ufs_plan_matches_native_lan(m.comp,&p));
    assert(ufs_browser_plan_for_screen(m.comp,124,&a,&from_save)==2);
    assert(from_save.volume_count==2 && from_save.volumes[1].members==2);
    assert(ufs_commit_partitions(m.comp,&p,&native,&cr)==0);
    assert(cr.created_markers==4 && cr.before_gq==90 && cr.after_gq==294);
    assert(ufs_verify_fresh_layout(m.comp,&p,90)==0);
    assert(ufs_browser_plan_for_screen(m.comp,124,&a,&from_save)==1);
    assert(a.mode=='G' && a.data1==2 && a.volume_slot==1 &&
           a.begin_gq==193 && a.end_gq==293 && a.marker_count==4);
    assert(from_save.volumes[1].members==2 && from_save.volumes[1].id.key==2);
    assert(ufs_browser_view_for_screen(m.comp,129,&b)==1);
    assert(b.mode=='G' && b.begin_gq==a.begin_gq && b.end_gq==a.end_gq);
    assert(ufs_browser_view_for_screen(m.comp,103,&priv)==1 &&
           priv.mode=='P' && priv.begin_gq==91 && priv.end_gq==191);
    assert(ufs_browser_view_for_screen(m.comp,132,&root)==1 &&
           root.mode=='R' && root.begin_gq==0 && root.end_gq==90);
    assert(ufs_populate_private(m.comp,&from_save,24,2,&native,&sr)==0);
    assert(sr.files_added>=7 && sr.files_added<=11 && sr.marker_stamped);
    n_before=m.next_file;
    /* Group's second member must not allocate or duplicate anything. */
    assert(ufs_populate_private(m.comp,&from_save,29,2,&native,&sr)==1);
    assert(m.next_file==n_before);
    assert(ufs_populate_private(m.comp,&from_save,3,1,&native,&sr)==0);
    assert(ufs_verify_active_layout(m.comp,&from_save,90)==0);
    assert(ufs_browser_view_for_screen(m.comp,124,&a)==1);
    assert(ufs_browser_view_for_screen(m.comp,129,&b)==1);
    assert(ufs_browser_resolve_row(m.comp,&a,0,&row)==1 && row.is_file &&
           row.first_gq==193);
    assert(ufs_browser_resolve_row(m.comp,&b,0,&(struct ufs_browser_row){0})==1);
    assert(ufs_safe_delete_file(m.comp,&a,&row,&dr)==0);
    assert(ufs_compact_native(m.comp,&a,&rr)==0);
    assert(ufs_browser_view_for_screen(m.comp,129,&b)==1);
    assert(ufs_verify_active_layout(m.comp,&from_save,90)==0);
    assert(ufs_populate_private(m.comp,&from_save,29,2,&native,&sr)==1);
    assert(ufs_browser_view_for_screen(m.comp,103,&priv)==1);
    assert(ufs_browser_resolve_row(m.comp,&priv,0,&(struct ufs_browser_row){0})==1);
    /* Invalid UFS2 boundary invalidates all grouped readers. */
    m.memory_valid[192]=0;
    assert(ufs_browser_view_for_screen(m.comp,124,&a)==-1);
    assert(ufs_browser_view_for_screen(m.comp,129,&b)==-1);
    m.memory_valid[192]=1;
    assert(ufs_browser_view_for_screen(m.comp,124,&a)==1);
    /* Assert no root file or private P data leaked into group range. */
    for(i=0;i<90;i++)if(m.memory_valid[i])assert(m.memory[i]==0);
    puts("test_grouped_lan: PASS shared DATA1=2 disk, private DATA1=1, root, native deletion/repack, idempotence, bounds and corruption");
    return 0;
}
