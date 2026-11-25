#ifndef UFS_VANILLA_DELETE_TIMING_H
#define UFS_VANILLA_DELETE_TIMING_H
/*
 * File_Deleter receives n CPU ticks from the ORIGINAL SvbCycle scheduler.
 * It completes on the first call where progress+n >= numticksrequired.
 * Run at most numticksrequired-progress-1 ticks through vanilla Tick, then
 * replace only its unsafe terminal action. No wall-clock approximation.
 */
struct ufs_delete_tick_plan { int vanilla_ticks; int finish_now; };
int ufs_delete_tick_decide(int progress,int required,int incoming,
                           struct ufs_delete_tick_plan *out);
#endif
