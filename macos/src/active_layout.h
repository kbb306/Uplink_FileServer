/* Native DataBank safety validation for already-populated UFS2 partitions.
 * Read-only: does not call game methods or alter file contents.
 */
#ifndef UFS_ACTIVE_LAYOUT_H
#define UFS_ACTIVE_LAYOUT_H
#include "partition_commit.h"
/* Returns 0 if every file is completely contained in one volume, has exactly
 * its declared contiguous Gq, and all six immutable UFS2 markers are present
 * at their original positions. Negative => fail closed.
 */
int ufs_verify_active_layout(const void *computer,
                             const struct ufs_volume_plan *plan,
                             int root_gq);
#endif
