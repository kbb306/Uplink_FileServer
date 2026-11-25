#include "vanilla_delete_timing.h"
int ufs_delete_tick_decide(int progress,int required,int incoming,
                           struct ufs_delete_tick_plan *out){
    int remaining;
    if(!out||required<=0||required>100000000||progress<0||progress>=required||
       incoming<0||incoming>100000000)return -1;
    remaining=required-progress;
    out->finish_now=(incoming>=remaining);
    out->vanilla_ticks=out->finish_now ? remaining-1 : incoming;
    return 0;
}
