/* Pure matching policy for post-load bank inspection; no native writes. */
#ifndef UFS_POSTLOAD_GUARD_H
#define UFS_POSTLOAD_GUARD_H
struct ufs_postload_guard {
    unsigned long computer;
    unsigned long cluster;
    unsigned int node_count;
    unsigned int completed;
};
void ufs_postload_reset(struct ufs_postload_guard *,unsigned long computer,unsigned long cluster);
void ufs_postload_track_node(struct ufs_postload_guard *);
int ufs_postload_admit(struct ufs_postload_guard *,unsigned long computer,
                       unsigned long return_rva,unsigned long expected_return_rva);
#endif
