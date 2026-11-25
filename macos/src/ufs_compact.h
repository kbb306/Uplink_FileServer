/*
 * UFS2 partition-bounded file-reference compactor (portable C).
 * SPDX-License-Identifier: MIT
 *
 * This implementation NEVER changes Data objects, file-slot numbers,
 * capacity or any cell outside [start,end). Its sole mutation is updating
 * the selected region's DArray<int> memory references and validity bits.
 *
 * Caller MUST authenticate a complete UFS2 marker layout before calling,
 * and supply the exact size of each ordinary Data file via get_size().
 * An optional post-commit verifier triggers exact journal rollback.
 */
#ifndef UFS_COMPACT_H
#define UFS_COMPACT_H
#define UFS_COMPACT_MAX 4096

typedef int (*ufs_compact_file_size)(void *ctx,int file_slot);
typedef int (*ufs_compact_verify)(void *ctx);
struct ufs_compact_result {
    int start,end,files,used_gq,moved_files,freed_gaps;
};
/* Return 0 on success; <0 means no changes (or rolled back). */
static inline int ufs_compact_range(int total,int start,int end,int file_slots,
                             int *mem,unsigned char *valid,
                             ufs_compact_file_size get_size,void *ctx,
                             ufs_compact_verify verify,
                             struct ufs_compact_result *out){
    int orig[UFS_COMPACT_MAX],packed[UFS_COMPACT_MAX];
    unsigned char original_valid[UFS_COMPACT_MAX],packed_valid[UFS_COMPACT_MAX];
    int count,read_pos,write_pos,used=0,moved=0,files=0,extent=0,k,slot;
    if(!out||!mem||!valid||!get_size||total<=0||total>20000||
       start<0||start>=end||end>total||file_slots<1||file_slots>4096 ||
       end-start>UFS_COMPACT_MAX)return -1;
    count=end-start;
    for(k=0;k<count;k++){
        orig[k]=mem[start+k];original_valid[k]=valid[start+k];
        packed[k]=0;packed_valid[k]=0;
    }
    write_pos=0;
    for(read_pos=0;read_pos<count;){
        int i,len,absolute=start+read_pos;
        if(!original_valid[read_pos]){read_pos++;continue;}
        slot=orig[read_pos];
        if(slot<0||slot>=file_slots)return -2;
        len=get_size(ctx,slot);
        if(len<1||len>count-read_pos || len>count-write_pos)return -3;
        /* A file's size in its native Data record must equal every physical
         * reference; no aliasing, overlaps, split files or foreign refs. */
        for(i=0;i<len;i++)
            if(!original_valid[read_pos+i]||orig[read_pos+i]!=slot)return -4;
        for(i=0;i<total;i++){
            if(i>=absolute&&i<absolute+len)continue;
            if(valid[i]&&mem[i]==slot)return -5;
        }
        for(i=0;i<len;i++){packed_valid[write_pos+i]=1;packed[write_pos+i]=slot;}
        if(write_pos!=read_pos)moved++;
        write_pos+=len;read_pos+=len;used+=len;files++;
    }
    /* Already compact: validate only, never touch this bank. */
    if(!moved){
        out->start=start;out->end=end;out->files=files;out->used_gq=used;
        out->moved_files=0;out->freed_gaps=count-used;
        return 0;
    }
    for(k=0;k<count;k++){valid[start+k]=packed_valid[k];mem[start+k]=packed[k];}
    /* Verify every expected byte, and also optionally verify the entire
     * UFS2 layout, BEFORE accepting the mutation. */
    for(k=0;k<count;k++)
        if(valid[start+k]!=packed_valid[k]||
           (packed_valid[k]&&mem[start+k]!=packed[k])){extent=-6;break;}
    if(extent==0 && verify && !verify(ctx))extent=-7;
    if(extent<0){
        for(k=0;k<count;k++){valid[start+k]=original_valid[k];mem[start+k]=orig[k];}
        return extent;
    }
    out->start=start;out->end=end;out->files=files;out->used_gq=used;
    out->moved_files=moved;out->freed_gaps=count-used;
    return 0;
}
#endif
