/*
 * Uplink Mac 1.6.9 v0.16: generic native-LAN UFS2-validated, optionally clickable FileServer rows.
 * No native code-page patching; only a verified UFS2 FileClick can target an operation.
 */
#ifndef UFS_BROWSER_ROUTE_H
#define UFS_BROWSER_ROUTE_H
#include "partition_commit.h"

struct ufs_browser_view {
    int node_index;       /* Native LAN system ID, or main server's ID. */
    int screen_index;     /* Native RemoteInterface.currentScreenIndex */
    int mode;             /* 'R' root, 'P' private, 'G' grouped */
    int data1;
    int volume_slot;     /* physical volume index or -1 for root */
    int begin_gq;        /* inclusive native memory address */
    int end_gq;          /* exclusive native memory address */
    int root_gq;
    int bank_gq;
    int total_files;
    int marker_count;
};

/* 1 = verified partitioned custom-LAN file screen;
 * 2 = valid markerless candidate for EXPLICIT saved-LAN migration;
 * 0 = unrelated or vanilla/root-only LAN screen;
 * -1 = inconsistent or corrupt partition layout. Read-only. */
int ufs_browser_view_for_screen(const void *computer, int screen_index,
                                struct ufs_browser_view *out);
/* Read-only native system-DArray reconciliation for the fresh LoadLAN hook;
 * prevents committing a plan that differs from the finished LAN objects. */
int ufs_plan_matches_native_lan(const void *computer,
                                const struct ufs_volume_plan *plan);
/* Same validated lookup; optionally returns reconstructed DATA1 plan for
 * saved-game population and group membership. */
int ufs_browser_plan_for_screen(const void *computer, int screen_index,
                               struct ufs_browser_view *out,
                               struct ufs_volume_plan *out_plan);

/* Compute the row number in the native full-bank browser that corresponds to
 * `display_row` within the selected partition. Return 1 if found, 0 if the
 * partition has fewer rows, or -1 if the native bank is malformed. Returns
 * absolute native row indices, NOT memory indices. No native mutation.
 */
struct ufs_browser_row {
    int native_row;
    int visible_count;
    int first_gq;
    int length_gq;
    int is_file;
};
/* Select one row in an immutable, validated partition; returns its native
 * row ordinal and absolute native Gq range. 1=visible, 0=out of bounds,
 * negative=fail closed. This is used to authorize FileClick safely.
 */
int ufs_browser_resolve_row(const void *computer,const struct ufs_browser_view *view,
                            int display_row,struct ufs_browser_row *out);
int ufs_browser_map_row(const void *computer, const struct ufs_browser_view *view,
                        int display_row, int *native_row, int *visible_count);
#endif
