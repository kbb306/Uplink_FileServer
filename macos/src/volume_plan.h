/*
 * DATA1 partition plan (no native save or bank writes).
 * C99, host-independent, deterministic. This is the first stage of the Mac port
 * of Windows v0.7C2's native-Databank partition model.
 */
#ifndef UFS_VOLUME_PLAN_H
#define UFS_VOLUME_PLAN_H
#define UFS_MAX_VOLUMES 64
#define UFS_PRIVATE_CAPACITY 100
#define UFS_MAX_NODE_INDEX 4095

struct ufs_volume_key {
    char mode;  /* R = root, P = per-node private, G = group */
    unsigned int key;
};
struct ufs_volume {
    struct ufs_volume_key id;
    int first_node;
    unsigned int members;
    int capacity_gq;
    int begin_marker;  /* proposed native absolute memory index, uncommitted */
    int end_marker;    /* proposed native absolute memory index, uncommitted */
};
struct ufs_volume_plan {
    unsigned long cluster_id;
    unsigned int volume_count;
    unsigned int fileserver_count;
    unsigned int root_fileserver_count;
    int root_size_gq;
    int proposed_native_total_gq;
    struct ufs_volume volumes[UFS_MAX_VOLUMES];
};
void ufs_plan_reset(struct ufs_volume_plan *p, unsigned long cluster_id);
int ufs_mode_to_key(int data1, int node_index, struct ufs_volume_key *out);
/* Return -1 on inconsistent/excessive inputs, 0 for root, 1 for private/group.
 * out_volume is index within plan for private/group, -1 for root.
 */
int ufs_plan_register(struct ufs_volume_plan *p, int node_index, int data1,
                      int *out_volume);
void ufs_marker_name(char dst[17], char boundary, struct ufs_volume_key id);
/* Plan future native DataBank boundaries. Does not modify actual memory. */
int ufs_plan_ranges(struct ufs_volume_plan *p, int original_root_size_gq);
/* Returns inclusive start and exclusive end, never exposing marker indices. */
int ufs_select_range(const struct ufs_volume_plan *p,int node_index,int data1,
                     int *begin,int *end);
/* Pure placement logic; slots are 0=unused, 1=occupied, no native pointers. */
int ufs_find_placement(const unsigned char *occupied,int total,int begin,int end,
                       int file_size);

#endif
