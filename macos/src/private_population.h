/* v0.16 - private and grouped FILESERVER initial population.
 * Uses the same verified native Data constructor/PutData-at-index ABI as
 * partition_commit; does not touch main executable code or saved files.
 */
#ifndef UFS_PRIVATE_POPULATION_H
#define UFS_PRIVATE_POPULATION_H
#include "partition_commit.h"

/* A sentinel in the START boundary marker's *Data::version*, which Uplink
 * serializes as a float. Marker TYPE/SIZE/encryption/coordinates are unchanged.
 * No extra hidden file or Gq of capacity is consumed. */
#define UFS_PRIVATE_SEEDED_VERSION 57.0f

struct ufs_private_seed_report {
    int node;
    int before_files;
    int after_files;
    int existing_files;
    int files_added;
    int gq_added;
    int skipped_no_room;
    int begin_gq;
    int end_gq;
    int marker_stamped;
};

/* 0 generated (may add 0 if full); 1 already generated; <0 error.
 * All input state must describe the validated native LAN with valid UFS2
 * boundaries. Calls must be made on the game's thread, with native ABI
 * validation already completed by ufs_mac.c. */
int ufs_populate_private(void *computer, const struct ufs_volume_plan *plan,
                         int node_index, int data1, const struct ufs_native_writes *native,
                         struct ufs_private_seed_report *out);

#endif
