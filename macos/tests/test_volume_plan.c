#include <stdio.h>
#include <string.h>
#include "../src/volume_plan.h"
#define VERIFY(x) do {if(!(x)){fprintf(stderr,"FAIL at line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(void) {
    struct ufs_volume_plan plan;
    struct ufs_volume_key key;
    char marker[17];
    int ix,mode;
    ufs_plan_reset(&plan,0x1234u);
    VERIFY(plan.volume_count==0 && plan.cluster_id==0x1234u);
    VERIFY(ufs_plan_register(&plan,0,-1,&ix)==0 && ix==-1);
    VERIFY(ufs_plan_register(&plan,1,0,&ix)==0 && ix==-1);
    VERIFY(ufs_plan_register(&plan,3,1,&ix)==1 && ix==0);
    VERIFY(ufs_plan_register(&plan,16,1,&ix)==1 && ix==1);
    VERIFY(plan.volumes[0].id.mode=='P' && plan.volumes[0].id.key==3);
    VERIFY(plan.volumes[1].id.mode=='P' && plan.volumes[1].id.key==16);
    VERIFY(ufs_plan_register(&plan,24,2,&ix)==1 && ix==2);
    VERIFY(ufs_plan_register(&plan,29,2,&ix)==1 && ix==2);
    VERIFY(plan.volumes[2].members==2 && plan.volumes[2].capacity_gq==100);
    VERIFY(ufs_plan_register(&plan,30,3,&ix)==1 && ix==3);
    VERIFY(plan.volume_count==4 && plan.root_fileserver_count==2);
    VERIFY(ufs_mode_to_key(1,3,&key)==1);
    ufs_marker_name(marker,'B',key);
    VERIFY(strcmp(marker,"UFS2.B.P00000003")==0);
    ufs_marker_name(marker,'E',key);
    VERIFY(strcmp(marker,"UFS2.E.P00000003")==0);
    VERIFY(ufs_mode_to_key(2,29,&key)==1);
    ufs_marker_name(marker,'B',key);
    VERIFY(strcmp(marker,"UFS2.B.G00000002")==0);
    VERIFY(ufs_mode_to_key(-1,3,&key)==0 && key.mode=='R');
    VERIFY(ufs_mode_to_key(-2,3,&key)==-1);
    VERIFY(ufs_mode_to_key(1,-1,&key)==-1);
    VERIFY(ufs_mode_to_key(1,4096,&key)==-1);
    for(ix=0;ix<2;ix++){
        ufs_plan_reset(&plan,(unsigned long)(ix+1));
        for(mode=0;mode<UFS_MAX_VOLUMES;mode++){
            VERIFY(ufs_plan_register(&plan,mode,1,&(int){0})==1);
        }
        VERIFY(plan.volume_count==UFS_MAX_VOLUMES);
        VERIFY(ufs_plan_register(&plan,80,1,&(int){0})==-1);
    }
    {
        unsigned char bank[1000]={0};int b,e,root_end;
        ufs_plan_reset(&plan,99);
        VERIFY(ufs_plan_register(&plan,3,1,&ix)==1);
        VERIFY(ufs_plan_register(&plan,16,-1,&ix)==0);
        VERIFY(ufs_plan_register(&plan,24,1,&ix)==1);
        VERIFY(ufs_plan_register(&plan,29,2,&ix)==1);
        VERIFY(ufs_plan_register(&plan,30,2,&ix)==1);
        VERIFY(ufs_plan_ranges(&plan,87)==0);
        VERIFY(plan.root_size_gq==87 && plan.proposed_native_total_gq==393);
        VERIFY(ufs_select_range(&plan,3,1,&b,&e)==1 && b==88 && e==188);
        VERIFY(ufs_select_range(&plan,24,1,&b,&e)==1 && b==190 && e==290);
        VERIFY(ufs_select_range(&plan,29,2,&b,&e)==1 && b==292 && e==392);
        VERIFY(ufs_select_range(&plan,30,2,&b,&e)==1 && b==292 && e==392);
        VERIFY(ufs_select_range(&plan,16,-1,&b,&e)==0 && b==0 && e==87);
        root_end=e;
        bank[87]=bank[188]=bank[189]=bank[290]=bank[291]=bank[392]=1;
        VERIFY(ufs_find_placement(bank,393,0,root_end,6)==0);
        VERIFY(ufs_find_placement(bank,393,292,392,6)==292);
        for(int j=292;j<392;j++)bank[j]=1;
        VERIFY(ufs_find_placement(bank,393,292,392,1)==-1);
        VERIFY(ufs_find_placement(bank,393,190,290,100)==190);
        VERIFY(ufs_find_placement(bank,393,190,290,101)==-1);
        VERIFY(ufs_find_placement(bank,393,86,88,2)==-1);
        VERIFY(ufs_plan_ranges(&plan,19700)==-1);
    }
    puts("DATA1 volume and planned placement tests PASS");
    return 0;
}
