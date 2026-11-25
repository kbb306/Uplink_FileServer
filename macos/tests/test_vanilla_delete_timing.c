/* Compare terminal timing with the original SvbCycle/FileDeleter::Tick
 * semantics, including varying hardware total speed, CPU priority and size.
 * Native FileDeleter::SetTarget uses 9*Data.size on this exact Mac build.
 */
#include <assert.h>
#include <stdio.h>
#include "../src/vanilla_delete_timing.h"

static int run(int total_cpu_speed,double allocation,int gq,int safe){
    /* Native scheduler: every 100ms runs SvbCycle(GetCPUSpeed()/10).
       Each wrapper has a fractional priority accumulator. */
    const int percycle=total_cpu_speed/10;
    const int required=9*gq;
    int cycle,progress=0,status=0;
    double accum=0.0;
    for(cycle=1;cycle<400000;cycle++){
        int ticks=0;
        struct ufs_delete_tick_plan plan;
        accum+=percycle*allocation;
        if(accum>1.0){ticks=1+(int)(accum-1.0);accum-=(double)ticks;}
        if(!ticks)continue;
        if(status==0){status=1;continue;}
        if(!safe){
            progress+=ticks;
            if(progress>=required)return cycle;
        }else{
            assert(ufs_delete_tick_decide(progress,required,ticks,&plan)==0);
            if(!plan.finish_now){
                assert(plan.vanilla_ticks==ticks);
                progress+=plan.vanilla_ticks;
            }else{
                assert(plan.vanilla_ticks>=0);
                assert(progress+plan.vanilla_ticks<required);
                progress+=plan.vanilla_ticks;
                return cycle;
            }
        }
    }
    assert(!"Delete did not complete within simulation limit");
    return -1;
}
int main(void){
    const int cpus[]={10,30,60,100,180,360,1000};
    const double allocations[]={0.02,0.10,0.25,0.50,0.75,1.0};
    const int sizes[]={1,2,5,12,33,100};
    struct ufs_delete_tick_plan p;
    int a,b,c;
    for(a=0;a<(int)(sizeof(cpus)/sizeof(cpus[0]));a++)
        for(b=0;b<(int)(sizeof(allocations)/sizeof(allocations[0]));b++)
            for(c=0;c<(int)(sizeof(sizes)/sizeof(sizes[0]));c++){
                int orig=run(cpus[a],allocations[b],sizes[c],0);
                int mod=run(cpus[a],allocations[b],sizes[c],1);
                assert(orig==mod);
            }
    assert(ufs_delete_tick_decide(0,9,8,&p)==0 && p.vanilla_ticks==8 && !p.finish_now);
    assert(ufs_delete_tick_decide(8,9,1,&p)==0 && p.vanilla_ticks==0 && p.finish_now);
    assert(ufs_delete_tick_decide(5,9,100,&p)==0 && p.vanilla_ticks==3 && p.finish_now);
    assert(ufs_delete_tick_decide(9,9,1,&p)<0);
    assert(ufs_delete_tick_decide(0,0,1,&p)<0);
    assert(ufs_delete_tick_decide(0,9,-1,&p)<0);
    puts("PASS: native scheduler equivalence for CPU totals, allocation priorities and file sizes (252 cases)");
    return 0;
}
