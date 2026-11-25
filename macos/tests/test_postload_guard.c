#include <stdio.h>
#include "../src/postload_guard.h"
#define CHECK(e) do {if(!(e)){fprintf(stderr,"postload guard failed at %d: %s\n",__LINE__,#e);return 1;}}while(0)
int main(void){
 struct ufs_postload_guard g={0};
 CHECK(!ufs_postload_admit(&g,0x100,0x200,0x200));
 ufs_postload_reset(&g,0x1000,0x2000);
 CHECK(!ufs_postload_admit(&g,0x1000,0x200,0x200));
 ufs_postload_track_node(&g);
 CHECK(!ufs_postload_admit(&g,0x1001,0x200,0x200));
 CHECK(!ufs_postload_admit(&g,0x1000,0x201,0x200));
 CHECK(ufs_postload_admit(&g,0x1000,0x200,0x200));
 CHECK(!ufs_postload_admit(&g,0x1000,0x200,0x200));
 ufs_postload_reset(&g,0x1200,0x2000); /* cluster address can be reused */
 CHECK(!ufs_postload_admit(&g,0x1000,0x200,0x200));
 ufs_postload_track_node(&g);
 CHECK(ufs_postload_admit(&g,0x1200,0x200,0x200));
 puts("v0.7 postload guard PASS");return 0;
}
