/* Existing vanilla File_Deleter/File_Copier workflows, limited to a
 * verified UFS2 FILESERVER/root/group view. The caller authenticates the UFS2
 * layout and selected row before evaluating these pure policy predicates.
 * No new gameplay tools or remote Defrag implementation. */
#ifndef UFS_OPERATION_POLICY_H
#define UFS_OPERATION_POLICY_H

enum ufs_operation_action {
    UFS_OP_BLOCKED=0,
    UFS_OP_NATIVE_FREE_DESTINATION=1,
    UFS_OP_TIMED_SAFE_DELETE=2,
    UFS_OP_NATIVE_FILE_COPIER_SOURCE=3
};

/* Shared admission check: timed File_Deleter must support the same
 * authenticated disk modes as the click policy (P, R, and G). */
int ufs_deleter_arm_mode_supported(char mode);

enum ufs_operation_action ufs_operation_decide(int operations_enabled,
    int safe_delete_enabled, int is_file, int target_file_deleter,
    int target_file_copier, char mode);
#endif
