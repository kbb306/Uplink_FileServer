/*
 * Read-only native Mac Uplink 1.6.9 x86_64 DataBank inspection.
 * An exact executable fingerprint must pass before this is used in game.
 *
 * Recovered from the native constructor at RVA 0xe1b16 (Computer::databank),
 * methods 0xe8070/0xe8090/0xe8cf0/0xe91c0/0xe91f0, and the native FILESERVER
 * row enumerator at 0x5ca30. NOT Windows 1.55 offsets.
 *
 * NO mutations. No change to save files, file table, or code pages.
 */
#ifndef UFS_NATIVE_BANK_H
#define UFS_NATIVE_BANK_H
#include "volume_plan.h"

#define UFS_COMPUTER_BANK_OFFSET 0x128u
#define UFS_MAC_DATA_TITLE_OFFSET 0x08u
#define UFS_MAC_DATA_TYPE_OFFSET 0x48u
#define UFS_MAC_DATA_SIZE_OFFSET 0x4cu
#define UFS_MAC_DATA_ENCRYPTED_OFFSET 0x50u
#define UFS_MAC_DATA_COMPRESSED_OFFSET 0x54u
#define UFS_NATIVE_LIMIT_SLOTS 20000
#define UFS_NATIVE_LIMIT_FILES 4096

/* <0 error; 0 no markers; 1 validated marker layout. */
struct ufs_bank_report {
    int bank_gq;        /* native DataBank::GetSize = *(bank+0x48) */
    int file_slots;     /* native DataBank::GetDataSize = *(bank+0x18) */
    int file_count;
    int used_gq;
    int formatted;
    int ufs_marker_count;
    int orphan_markers;
    int layout_state;
    int first_marker_gq;
    int found_volumes;
};

/* When plan is incomplete (during generation), marker counts and bank shape
 * are still safe to inspect; existing markers cannot be accepted as a valid
 * saved layout until every FILESERVER has been enumerated.
 * Return 0 on successful observation, negative on structural anomalies.
 */
int ufs_bank_inspect(const void *computer, const struct ufs_volume_plan *plan,
                     struct ufs_bank_report *out);

/* Exact Intel method entry fingerprints to guard native-bank offsets. */
int ufs_bank_machine_signature(const unsigned char *image);

#endif
