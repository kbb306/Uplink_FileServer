#include "volume_plan.h"

void ufs_plan_reset(struct ufs_volume_plan *p, unsigned long cluster_id) {
    unsigned int i;
    p->cluster_id=cluster_id;
    p->volume_count=0;
    p->fileserver_count=0;
    p->root_fileserver_count=0;
    p->root_size_gq=0;
    p->proposed_native_total_gq=0;
    for(i=0;i<UFS_MAX_VOLUMES;i++) {
        p->volumes[i].id.mode=0;
        p->volumes[i].id.key=0;
        p->volumes[i].first_node=-1;
        p->volumes[i].members=0;
        p->volumes[i].capacity_gq=0;
        p->volumes[i].begin_marker=-1;
        p->volumes[i].end_marker=-1;
    }
}
int ufs_mode_to_key(int data1, int node_index, struct ufs_volume_key *out) {
    if (!out || node_index < 0 || node_index > UFS_MAX_NODE_INDEX) return -1;
    if (data1 < -1)return -1;
    if (data1 <= 0) {out->mode='R'; out->key=0; return 0;}
    if (data1 == 1) {out->mode='P'; out->key=(unsigned int)node_index; return 1;}
    out->mode='G';out->key=(unsigned int)data1;return 1;
}
int ufs_plan_register(struct ufs_volume_plan *p,int node_index,int data1,
                      int *out_volume) {
    struct ufs_volume_key id;
    unsigned int i;
    int result;
    if (!p || !out_volume)return -1;
    result=ufs_mode_to_key(data1,node_index,&id);
    if (result<0)return -1;
    if (p->fileserver_count>=UFS_MAX_NODE_INDEX+1)return -1;
    if (result==0){
        p->fileserver_count++;p->root_fileserver_count++;*out_volume=-1;return 0;
    }
    for(i=0;i<p->volume_count;i++) {
        if (p->volumes[i].id.mode==id.mode && p->volumes[i].id.key==id.key){
            p->volumes[i].members++;
            p->fileserver_count++;
            *out_volume=(int)i;
            return 1;
        }
    }
    if (p->volume_count>=UFS_MAX_VOLUMES)return -1;
    i=p->volume_count++;
    p->volumes[i].id=id;
    p->volumes[i].first_node=node_index;
    p->volumes[i].members=1;
    p->volumes[i].capacity_gq=UFS_PRIVATE_CAPACITY;
    p->fileserver_count++;
    *out_volume=(int)i;
    return 1;
}
void ufs_marker_name(char dst[17],char boundary,struct ufs_volume_key id) {
    static const char digits[]="0123456789ABCDEF";
    unsigned int i;
    dst[0]='U';dst[1]='F';dst[2]='S';dst[3]='2';dst[4]='.';
    dst[5]=boundary;dst[6]='.';dst[7]=id.mode;
    for(i=0;i<8;i++)dst[8+i]=digits[(id.key>>((7-i)*4))&15u];
    dst[16]=0;
}

int ufs_plan_ranges(struct ufs_volume_plan *p,int original_root_size_gq) {
    unsigned int i;
    int next;
    if(!p || original_root_size_gq<1 || original_root_size_gq>20000)return -1;
    if(p->volume_count>(unsigned int)((20000-original_root_size_gq)/(UFS_PRIVATE_CAPACITY+2)))
        return -1;
    p->root_size_gq=original_root_size_gq;
    next=original_root_size_gq;
    for(i=0;i<p->volume_count;i++){
        p->volumes[i].begin_marker=next;
        p->volumes[i].end_marker=next+UFS_PRIVATE_CAPACITY+1;
        next+=UFS_PRIVATE_CAPACITY+2;
    }
    p->proposed_native_total_gq=next;
    return 0;
}
int ufs_select_range(const struct ufs_volume_plan *p,int node_index,int data1,
                     int *begin,int *end) {
    struct ufs_volume_key id;
    unsigned int i;
    int mode;
    if(!p || !begin || !end || p->root_size_gq<=0)return -1;
    mode=ufs_mode_to_key(data1,node_index,&id);
    if(mode<0)return -1;
    if(mode==0){*begin=0;*end=p->root_size_gq;return 0;}
    for(i=0;i<p->volume_count;i++){
        if(p->volumes[i].id.mode==id.mode && p->volumes[i].id.key==id.key){
            if(p->volumes[i].begin_marker<0 || p->volumes[i].end_marker<0)return -1;
            *begin=p->volumes[i].begin_marker+1;
            *end=p->volumes[i].end_marker;
            return 1;
        }
    }
    return -1;
}
int ufs_find_placement(const unsigned char *occupied,int total,int begin,int end,
                       int file_size) {
    int i,j,free_run;
    if(!occupied || total<0 || total>20000 || begin<0 || end<begin || end>total ||
       file_size<1 || file_size>end-begin)return -1;
    for(i=begin;i<=end-file_size;i++){
        free_run=1;
        for(j=0;j<file_size;j++)if(occupied[i+j]){free_run=0;break;}
        if(free_run)return i;
    }
    return -1;
}
