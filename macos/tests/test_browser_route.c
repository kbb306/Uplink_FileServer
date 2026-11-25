/* Synthetic native x86_64 Mac layout: one root file, 6 UFS2 guard files. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/browser_route.h"

static void put_i(unsigned char *obj,int pos,int x){memcpy(obj+pos,&x,4);}
static void put_p(unsigned char *obj,int pos,void *p){memcpy(obj+pos,&p,8);}
int main(void){
    unsigned char *comp=(unsigned char*)calloc(1,0x2c8);
    unsigned char *file[8],*sys[34];
    unsigned long *files=(unsigned long*)calloc(16,sizeof(unsigned long));
    unsigned char *fvalid=(unsigned char*)calloc(16,1);
    int *memory=(int*)calloc(396,sizeof(int));
    unsigned char *mvalid=(unsigned char*)calloc(396,1);
    unsigned long *systems=(unsigned long*)calloc(34,sizeof(unsigned long));
    unsigned char *svalid=(unsigned char*)calloc(34,1);
    unsigned char *bank=comp+0x128;
    struct ufs_browser_view v;
    int row,count,status,i;
    const int positions[]={90,191,192,293,294,395};
    const char *names[]={"UFS2.B.P00000003","UFS2.E.P00000003",
       "UFS2.B.P00000018","UFS2.E.P00000018",
       "UFS2.B.P0000001D","UFS2.E.P0000001D"};
    assert(comp && files && fvalid && memory && mvalid && systems && svalid);
    put_i(comp,0x20,64);
    strcpy((char*)comp+0x24,"Encom R&D Network");
    strcpy((char*)comp+0x64,"Encom");
    strcpy((char*)comp+0xa4,"202.520.10.089");
    put_i(bank,0x18,16);put_i(bank,0x48,396);
    put_p(bank,0x28,files);put_p(bank,0x30,fvalid);
    put_p(bank,0x58,memory);put_p(bank,0x60,mvalid);
    file[0]=(unsigned char*)calloc(1,0x60);
    strcpy((char*)file[0]+0x08,"Private memo");
    put_i(file[0],0x48,2);put_i(file[0],0x4c,6);
    files[0]=(unsigned long)file[0];fvalid[0]=1;
    for(i=0;i<6;i++){memory[i]=0;mvalid[i]=1;}
    for(i=0;i<6;i++){
        file[i+1]=(unsigned char*)calloc(1,0x60);
        strcpy((char*)file[i+1]+0x08,names[i]);
        put_i(file[i+1],0x48,1);put_i(file[i+1],0x4c,1);
        files[i+1]=(unsigned long)file[i+1];fvalid[i+1]=1;
        memory[positions[i]]=i+1;mvalid[positions[i]]=1;
    }
    put_i(comp,0x268+0x10,34);put_p(comp,0x268+0x20,systems);
    put_p(comp,0x268+0x28,svalid);
    for(i=0;i<34;i++)sys[i]=NULL;
    /* DArray slot index, native file browser screen, data1. */
    const int nodes[]={3,16,24,29,32};
    const int screens[]={103,116,124,129,132};
    const int data1[]={1,-1,1,1,-1};
    for(i=0;i<5;i++){
        int n=nodes[i];
        sys[n]=(unsigned char*)calloc(1,0x70);
        put_i(sys[n],0x08,n==32?5:7);
        put_i(sys[n],0x20,screens[i]-4);
        put_i(sys[n],0x24,n==32?-1:screens[i]);
        put_i(sys[n],0x28,data1[i]);
        systems[n]=(unsigned long)sys[n];svalid[n]=1;
    }
    status=ufs_browser_view_for_screen(comp,103,&v);
    assert(status==1 && v.mode=='P' && v.node_index==3 &&
           v.begin_gq==91 && v.end_gq==191);
    status=ufs_browser_map_row(comp,&v,0,&row,&count);
    assert(status==1 && row==3 && count==1);
    status=ufs_browser_map_row(comp,&v,1,&row,&count);
    assert(status==0 && row==-1 && count==1);
    status=ufs_browser_view_for_screen(comp,116,&v);
    assert(status==1 && v.mode=='R' && v.end_gq==90);
    status=ufs_browser_map_row(comp,&v,0,&row,&count);
    assert(status==1 && row==0 && count==2);
    status=ufs_browser_map_row(comp,&v,1,&row,&count);
    assert(status==1 && row==1 && count==2);
    status=ufs_browser_map_row(comp,&v,2,&row,&count);
    assert(status==0 && row==-1 && count==2);
    status=ufs_browser_view_for_screen(comp,124,&v);
    assert(status==1 && v.mode=='P' && v.begin_gq==193 && v.end_gq==293);
    status=ufs_browser_map_row(comp,&v,0,&row,&count);
    assert(status==1 && row==6 && count==1);
    status=ufs_browser_view_for_screen(comp,129,&v);
    assert(status==1 && v.mode=='P' && v.begin_gq==295 && v.end_gq==395);
    status=ufs_browser_map_row(comp,&v,0,&row,&count);
    assert(status==1 && row==9 && count==1);
    status=ufs_browser_view_for_screen(comp,132,&v);
    assert(status==1 && v.mode=='R' && v.node_index==32); /* native Main Server */
    assert(ufs_browser_view_for_screen(comp,1,&v)==0);
    /* v0.9: a completed upload in node 3 must NOT invalidate the disk.
     * The original v0.8 fresh-layout verifier rejected any occupied
     * private Gq, and would have hidden the disk after the first upload. */
    file[7]=(unsigned char*)calloc(1,0x60);
    strcpy((char*)file[7]+0x08,"Uploaded test file");
    put_i(file[7],0x48,2);put_i(file[7],0x4c,5);
    files[7]=(unsigned long)file[7];fvalid[7]=1;
    for(i=93;i<98;i++){memory[i]=7;mvalid[i]=1;}
    assert(ufs_browser_view_for_screen(comp,103,&v)==1);
    {
        struct ufs_browser_row a;
        assert(ufs_browser_resolve_row(comp,&v,0,&a)==1 &&
               a.first_gq==91 && a.length_gq==2 && !a.is_file && a.visible_count==3);
        assert(ufs_browser_resolve_row(comp,&v,1,&a)==1 &&
               a.first_gq==93 && a.length_gq==5 && a.is_file && a.visible_count==3);
        assert(ufs_browser_resolve_row(comp,&v,2,&a)==1 &&
               a.first_gq==98 && a.length_gq==93 && !a.is_file && a.visible_count==3);
        assert(ufs_browser_resolve_row(comp,&v,3,&a)==0 && a.visible_count==3);
    }
    assert(ufs_browser_view_for_screen(comp,124,&v)==1);
    assert(ufs_browser_resolve_row(comp,&v,0,&(struct ufs_browser_row){0})==1);
    /* A single file extending over the last marker must be rejected. */
    put_i(file[7],0x4c,101);
    assert(ufs_browser_view_for_screen(comp,103,&v)==-1);
    put_i(file[7],0x4c,5);
    assert(ufs_browser_view_for_screen(comp,103,&v)==1);
    /* Deleted file frees its blocks and returns to one 100-Gq row. */
    fvalid[7]=0;
    for(i=93;i<98;i++)mvalid[i]=0;
    assert(ufs_browser_view_for_screen(comp,103,&v)==1);
    assert(ufs_browser_resolve_row(comp,&v,0,&(struct ufs_browser_row){0})==1);
    /* Fail closed on corrupted/absent UFS2 metadata. */
    mvalid[293]=0;
    assert(ufs_browser_view_for_screen(comp,124,&v)==-1);
    mvalid[293]=1;
    strcpy((char*)comp+0x24,"Other network");
    assert(ufs_browser_view_for_screen(comp,103,&v)==1);
    /* Non-LAN computers must never be read as LanComputer objects. */
    put_i(comp,0x20,2);
    assert(ufs_browser_view_for_screen(comp,103,&v)==0);
    for(i=0;i<8;i++)free(file[i]);
    for(i=0;i<34;i++)free(sys[i]);
    free(files);free(fvalid);free(memory);free(mvalid);free(systems);free(svalid);free(comp);
    puts("test_browser_route: PASS populated private, click Gq, root, main, marker isolation, corruption");
    return 0;
}
