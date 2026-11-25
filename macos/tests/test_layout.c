#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/lan_layout.h"
static void fake_set_next(void *p,int index){ufs_store_int((unsigned char*)p+0xc8,index);}
static void fake_set_title(void *p,const char *title){strncpy((char *)p+8,title,63);}
static int test_success(void){
    unsigned char comp[0x130]={0}, cluster[0x40]={0}, node[0x68]={0}, password[0x110]={0},file[0xe8]={0};
    unsigned char system_flags[16]={0},screen_flags[16]={0};
    void *system_ptrs[16]={0},*screen_ptrs[16]={0};
    unsigned long v;
    v=16;memcpy(cluster+0x10,&v,8);memcpy(comp+0x108,&v,8);
    v=(unsigned long)system_ptrs;memcpy(cluster+0x20,&v,8);
    v=(unsigned long)system_flags;memcpy(cluster+0x28,&v,8);
    v=(unsigned long)screen_ptrs;memcpy(comp+0x118,&v,8);
    v=(unsigned long)screen_flags;memcpy(comp+0x120,&v,8);
    system_flags[3]=1;system_ptrs[3]=node;
    screen_flags[7]=1;screen_flags[10]=1;
    screen_ptrs[7]=password;screen_ptrs[10]=file;
    ufs_store_int(node+0x08,5);ufs_store_int(node+0x20,6);
    if(ufs_convert_main_to_file(comp,cluster,3,fake_set_next,fake_set_title)!=1)return 0;
    if(ufs_load_int(node+0x08)!=7||ufs_load_int(node+0x20)!=7||
       ufs_load_int(node+0x24)!=10||ufs_load_int(password+0xc8)!=10||strcmp((char*)password+8,"File Server")!=0 || strcmp((char*)file+8,"File Server")!=0)return 0;
    if(ufs_convert_main_to_file(comp,cluster,3,fake_set_next,fake_set_title)!=0)return 0;
    if(ufs_convert_main_to_file(comp,cluster,-1,fake_set_next,fake_set_title)!=0)return 0;
    return 1;
}
int main(void){if(!test_success()){fputs("layout conversion FAIL\n",stderr);return 1;}puts("layout conversion PASS");return 0;}
