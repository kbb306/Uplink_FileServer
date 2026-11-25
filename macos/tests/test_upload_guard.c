/* This models the exact occupied-memory guard logic in Uplink's native
 * DataBank::IsValidPlacement, as documented in its source (not runtime ABI).
 * Native FileCopier must still be live-tested on a disposable save. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/volume_plan.h"
static int native_is_valid(const unsigned char *used,int cap,int at,int size){
    int i;
    if(at<0||size<=0)return 2;
    for(i=at;i<at+size;i++){
        if(i>=cap||i<0)return 2;
        if(used[i])return 1;
    }
    return 0;
}
int main(void){
    unsigned char used[396]={0};
    int guards[]={90,191,192,293,294,395};
    int i;
    for(i=0;i<6;i++)used[guards[i]]=1;
    /* From each private disk interior, no legal write may pass its end. */
    assert(native_is_valid(used,396,91,100)==0);
    assert(native_is_valid(used,396,91,101)==1);
    assert(native_is_valid(used,396,193,100)==0);
    assert(native_is_valid(used,396,193,101)==1);
    assert(native_is_valid(used,396,295,100)==0);
    assert(native_is_valid(used,396,295,101)==1);
    assert(native_is_valid(used,396,89,2)==1);
    assert(native_is_valid(used,396,0,90)==0);
    /* An upload and a later deletion in one disk leave markers untouched. */
    memset(used+110,1,15);
    assert(ufs_find_placement(used,396,91,191,20)==125);
    assert(native_is_valid(used,396,110,15)==1);
    assert(native_is_valid(used,396,125,20)==0);
    memset(used+110,0,15);
    assert(native_is_valid(used,396,110,15)==0);
    for(i=0;i<6;i++)assert(used[guards[i]]==1);
    puts("test_upload_guard: PASS occupied UFS2 markers stop native-style crossing writes");
    return 0;
}
