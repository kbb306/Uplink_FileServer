/* v0.10: bounded partition-scoped file removal without native FileDeleter::Tick.
 * Exact Mac x86_64 DataBank layout, no heap allocations, no calls into Uplink.
 * Must only be called from an exact-fingerprint UI site after verification that
 * the user targeted native File_Deleter, on a disposable agent save.
 */
#ifndef UFS_SAFE_DELETE_H
#define UFS_SAFE_DELETE_H
#include "browser_route.h"
struct ufs_safe_delete_report {
    int file_slot;
    int first_gq, length_gq;
    int files_before, files_after;
    int used_before, used_after;
};
/* 0=committed; negative=refused, no write; -90=post-validation failed and
 * original state restored (may require discarding disposable trial save). */
int ufs_safe_delete_file(void *computer,
                          const struct ufs_browser_view *view,
                          const struct ufs_browser_row *row,
                          struct ufs_safe_delete_report *report);
#endif
