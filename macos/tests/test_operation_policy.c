#include <assert.h>
#include <stdio.h>
#include "../src/ufs_operation_policy.h"
int main(void){
    /* Identical vanilla tool routing for verified private and root views. */
    for(const char *m="PRG";*m;m++){
        char mode=*m;
        assert(ufs_deleter_arm_mode_supported(mode));
        assert(ufs_operation_decide(1,1,1,1,0,mode)==UFS_OP_TIMED_SAFE_DELETE);
        assert(ufs_operation_decide(1,0,1,1,0,mode)==UFS_OP_BLOCKED);
        assert(ufs_operation_decide(1,0,1,0,1,mode)==UFS_OP_NATIVE_FILE_COPIER_SOURCE);
        assert(ufs_operation_decide(1,1,0,1,1,mode)==UFS_OP_NATIVE_FREE_DESTINATION);
        assert(ufs_operation_decide(0,1,1,1,0,mode)==UFS_OP_BLOCKED);
        assert(ufs_operation_decide(1,1,1,0,0,mode)==UFS_OP_BLOCKED);
        assert(ufs_operation_decide(1,0,1,1,1,mode)==UFS_OP_BLOCKED);
    }
    assert(!ufs_deleter_arm_mode_supported('?'));
    assert(!ufs_deleter_arm_mode_supported('X'));
    assert(ufs_operation_decide(1,1,1,1,0,'?')==UFS_OP_BLOCKED);
    puts("operation policy: PASS P/R/G click-and-animation mode agreement, copier + deleter, opt-ins");
    return 0;
}
