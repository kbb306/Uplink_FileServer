/* Pure data-only part of Mac native FileServerScreen row isolation.
 * v0.9 retains separate Mac FileDraw UI routing; these pure row helpers remain for offline tests. */
#ifndef UFS_VOLUME_ROWS_H
#define UFS_VOLUME_ROWS_H
struct ufs_file_row {
    unsigned long file_identity; /* 0 = free run; otherwise native pointer */
    int start_gq;                /* ABSOLUTE native bank memory offset */
    int length_gq;
    int marker;                  /* nonzero = UFS2 boundary data */
};
/* Returns number of visible rows, or -1 if bad native data / output full.
 * Input file rows crossing a boundary are hidden; free rows may be clipped.
 * Output indices are ALWAYS ABSOLUTE. No writes to native DataBank.
 */
int ufs_filter_native_rows(const struct ufs_file_row *source, int count,
                           int exclusive_start, int exclusive_end,
                           struct ufs_file_row *out, int capacity);
#endif
