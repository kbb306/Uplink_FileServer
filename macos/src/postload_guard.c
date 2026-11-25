#include "postload_guard.h"
void ufs_postload_reset(struct ufs_postload_guard *s,unsigned long computer,unsigned long cluster){
    if(!s)return;
    s->computer=computer;s->cluster=cluster;s->node_count=0;s->completed=0;
}
void ufs_postload_track_node(struct ufs_postload_guard *s){
    if(s && s->computer && s->cluster && !s->completed && s->node_count<4096)s->node_count++;
}
int ufs_postload_admit(struct ufs_postload_guard *s,unsigned long computer,
                       unsigned long return_rva,unsigned long expected_return_rva){
    if(!s || !computer || !s->computer || !s->cluster || !s->node_count ||
       s->completed || computer!=s->computer || return_rva!=expected_return_rva)return 0;
    s->completed=1;
    return 1;
}
