/*
 * Mac Uplink 1.6.9 - v0.16 native DataBank boundary allocation.
 * No native storage operations occur in this module unless a caller supplies
 * a completely validated bank, an exact DATA1 plan and native method pointers.
 * UI and transfer isolation are validated independently by the browser route.
 */
#ifndef UFS_PARTITION_COMMIT_H
#define UFS_PARTITION_COMMIT_H
#include "native_bank.h"

typedef void (*ufs_bank_set_size_fn)(void *bank, int size);
typedef void (*ufs_bank_put_at_fn)(void *bank, void *file, int index);
typedef void *(*ufs_native_new_fn)(unsigned long bytes);
typedef void (*ufs_data_ctor_fn)(void *file);
typedef void (*ufs_data_title_fn)(void *file, const char *title);
struct ufs_native_writes {
    ufs_bank_set_size_fn set_size;
    ufs_bank_put_at_fn put_at;
    ufs_native_new_fn new_data;
    ufs_data_ctor_fn data_ctor;
    ufs_data_title_fn set_title;
};
struct ufs_commit_report {
    int before_gq, after_gq;
    int before_files, after_files;
    int before_used, after_used;
    int created_markers;
    int layout_state;
    int root_intact;
    int stage; /* 0 never mutated; 1 resized; 2 began inserting markers */
};
/* Validate canonical private/group/root counts in a LAN plan. */
int ufs_plan_is_valid(const struct ufs_volume_plan *plan);
/* 0 success; 1 layout already present; negative error (see source). A failure
 * after stage!=0 may leave an INCOMPLETE layout: DISCARD THE TEST SAVE.
 * This call MUST run on the game thread after native initialization and ONLY
 * with exact verified method RVAs. */
int ufs_commit_partitions(void *computer, struct ufs_volume_plan *plan,
                          const struct ufs_native_writes *native,
                          struct ufs_commit_report *result);
int ufs_verify_fresh_layout(const void *computer,
                             const struct ufs_volume_plan *plan,
                             int expected_root_gq);
#endif
