#include <stdio.h>
#include <string.h>
#include "../src/ufs_compact.h"
static int sizes[]={3,2,1,1,5};
static int size_of(void *ctx,int id){(void)ctx;return (id>=0&&id<5)?sizes[id]:-1;}
static int verify_yes(void *ctx){(void)ctx;return 1;}
static int verify_no(void *ctx){(void)ctx;return 0;}
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 int m[23]={0};unsigned char v[23]={0};struct ufs_compact_result r;
 /* root [0,8), marker at 8, private [9,20), marker at 20. */
 for(int i=0;i<3;i++)m[i]=0,v[i]=1;
 m[8]=2;v[8]=1;m[20]=3;v[20]=1;
 m[9]=1;m[10]=1;v[9]=v[10]=1;
 for(int i=14;i<19;i++)m[i]=4,v[i]=1;
 CHECK(ufs_compact_range(23,9,20,5,m,v,size_of,0,verify_yes,&r)==0);
 CHECK(r.moved_files==1 && r.files==2 && r.used_gq==7 && r.freed_gaps==4);
 CHECK(m[9]==1 && m[10]==1 && m[11]==4 && m[15]==4 && v[15]);
 CHECK(!v[16] && !v[19] && v[8] && v[20] && m[8]==2 && m[20]==3);
 CHECK(ufs_compact_range(23,9,20,5,m,v,size_of,0,verify_yes,&r)==0 && !r.moved_files);
 /* A fake extra reference outside the volume must be rejected unchanged. */
 m[22]=4;v[22]=1;
 CHECK(ufs_compact_range(23,9,20,5,m,v,size_of,0,verify_yes,&r)==-5);
 m[22]=0;v[22]=0;
 /* A failed postcheck must roll back every write. */
 for(int i=11;i<16;i++)m[i]=0,v[i]=0;
 for(int i=15;i<20;i++)m[i]=4,v[i]=1;
 int old[23];unsigned char oldv[23];memcpy(old,m,sizeof m);memcpy(oldv,v,sizeof v);
 CHECK(ufs_compact_range(23,9,20,5,m,v,size_of,0,verify_no,&r)==-7);
 CHECK(memcmp(m,old,sizeof m)==0 && memcmp(v,oldv,sizeof v)==0);
 /* A crossing file is never moved. */
 m[19]=4;v[19]=1;m[20]=4;v[20]=1;
 CHECK(ufs_compact_range(23,9,20,5,m,v,size_of,0,verify_yes,&r)<0);
 puts("ufs_compact core: PASS");return 0;
}
