#include "../src/ufs_compact.h"
int ufs_compact_export(int total,int start,int end,int file_slots,
                       int *mem,unsigned char *valid,
                       ufs_compact_file_size get_size,void *ctx,
                       ufs_compact_verify verify,
                       struct ufs_compact_result *out){
    return ufs_compact_range(total,start,end,file_slots,mem,valid,get_size,ctx,verify,out);
}
