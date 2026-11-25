/* Mac 1.6.9: remote deletion triggers bounded repack, not a Defrag tool.
 * Real DataBank records are represented by stable file-slot identities.
 * The unit model exercises the same UFS2 compactor used by the live mod.
 */
#include <stdio.h>
#include "../src/ufs_compact.h"
#define CHECK(e) do {if(!(e)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#e);return 1;}}while(0)
static int sizes[]={3,5,4,1,1,6};
static int size_of(void *c,int slot){(void)c;return slot>=0&&slot<6?sizes[slot]:-1;}
static int yes(void*c){(void)c;return 1;}
int main(void){
    int m[36]={0}; unsigned char valid[36]={0};
    struct ufs_compact_result r;
    /* Root [0,8); private A [9,24); private B [26,35); markers at 8,24,25,35. */
    for(int i=0;i<3;i++)m[i]=0,valid[i]=1;
    m[8]=3;m[24]=4;m[25]=3;m[35]=4;
    valid[8]=valid[24]=valid[25]=valid[35]=1;
    for(int i=9;i<14;i++)m[i]=1,valid[i]=1;
    for(int i=16;i<20;i++)m[i]=2,valid[i]=1;
    for(int i=26;i<32;i++)m[i]=5,valid[i]=1;
    /* Native File_Deleter terminal tick removes the 5-Gq private file,
       leaving a free-space gap [9,14); then the replacement repacks A. */
    for(int i=9;i<14;i++)m[i]=0,valid[i]=0;
    CHECK(ufs_compact_range(36,9,24,6,m,valid,size_of,0,yes,&r)==0);
    CHECK(r.files==1 && r.moved_files==1 && r.used_gq==4 && r.freed_gaps==11);
    for(int i=9;i<13;i++)CHECK(valid[i]&&m[i]==2);
    for(int i=13;i<24;i++)CHECK(!valid[i]);
    /* No marker, root file, or second private bank moved. */
    CHECK(m[8]==3&&m[24]==4&&m[25]==3&&m[35]==4);
    for(int i=0;i<3;i++)CHECK(valid[i]&&m[i]==0);
    for(int i=26;i<32;i++)CHECK(valid[i]&&m[i]==5);
    /* Same range idempotent, and unchanged slots stay stable. */
    CHECK(ufs_compact_range(36,9,24,6,m,valid,size_of,0,yes,&r)==0);
    CHECK(r.moved_files==0);
    /* Vanilla local File_Deleter skips remote repack; the new hook is only
       attached to selected remote ENCOM FileServer nodes in ufs_mac.c. */
    puts("PASS: remote post-delete pack; all surrounding disks/markers fixed");
    return 0;
}
