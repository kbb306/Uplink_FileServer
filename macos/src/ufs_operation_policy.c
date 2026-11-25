#include "ufs_operation_policy.h"
int ufs_deleter_arm_mode_supported(char mode){
    return mode=='P' || mode=='R' || mode=='G';
}

enum ufs_operation_action ufs_operation_decide(int operations_enabled,
    int safe_delete_enabled,int is_file,int target_file_deleter,
    int target_file_copier,char mode){
    if(!operations_enabled || !ufs_deleter_arm_mode_supported(mode))return UFS_OP_BLOCKED;
    if(!is_file)return UFS_OP_NATIVE_FREE_DESTINATION;
    if(target_file_deleter)return safe_delete_enabled?
        UFS_OP_TIMED_SAFE_DELETE:UFS_OP_BLOCKED;
    if(target_file_copier)return UFS_OP_NATIVE_FILE_COPIER_SOURCE;
    return UFS_OP_BLOCKED;
}
