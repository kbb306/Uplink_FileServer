#include <assert.h>
#include <stdio.h>
#include "../src/volume_rows.h"
int main(void){
    struct ufs_file_row all[]={
        {10,0,20,0},    /* root file */
        {0,20,80,0},    /* root free run */
        {11,100,1,1},  /* begin marker */
        {21,101,17,0}, /* first private file */
        {0,118,83,0},  /* first private free run */
        {12,201,1,1},  /* end marker */
        {0,202,101,0}, /* second private, guard + free */
        {13,303,1,1},  /* second end marker */
        {30,98,5,0}   /* malformed file crossing boundary */
    };
    struct ufs_file_row out[16];int n;
    n=ufs_filter_native_rows(all,9,0,100,out,16);
    assert(n==2 && out[0].file_identity==10 && out[1].start_gq==20);
    n=ufs_filter_native_rows(all,9,101,201,out,16);
    assert(n==2 && out[0].file_identity==21 && out[0].start_gq==101);
    assert(out[1].file_identity==0 && out[1].start_gq==118 && out[1].length_gq==83);
    n=ufs_filter_native_rows(all,9,203,303,out,16);
    assert(n==1 && out[0].file_identity==0 && out[0].start_gq==203 && out[0].length_gq==100);
    assert(ufs_filter_native_rows(all,9,101,201,out,1)==-1);
    assert(ufs_filter_native_rows(all,9,201,201,out,16)==-1);
    puts("PASS: private/root file rows, free-row clipping, hidden markers, absolute indices");
    return 0;
}
