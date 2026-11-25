#include "volume_rows.h"
int ufs_filter_native_rows(const struct ufs_file_row *source,int count,
                           int begin,int end,struct ufs_file_row *out,int cap){
    int i,n=0;
    if(!source||!out||count<0||count>20000||cap<0||cap>20000||
       begin<0||end<=begin||end>20000)return -1;
    for(i=0;i<count;i++){
        struct ufs_file_row row=source[i];
        int last;
        if(row.start_gq<0||row.length_gq<=0||
           row.start_gq>20000||row.length_gq>20000)return -1;
        last=row.start_gq+row.length_gq;
        if(last>20000)return -1;
        if(row.marker)continue;
        if(row.file_identity) {
            /* Hiding partly-crossing files is safer than returning the
             * wrong absolute file index to the native file copier. */
            if(row.start_gq<begin||last>end)continue;
        }else {
            if(last<=begin||row.start_gq>=end)continue;
            if(row.start_gq<begin)row.start_gq=begin;
            if(last>end)last=end;
            row.length_gq=last-row.start_gq;
        }
        if(n>=cap)return -1;
        out[n++]=row;
    }
    return n;
}
