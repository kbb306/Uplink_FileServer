#include "click_policy.h"
int ufs_click_policy_allow(int opted_in,int is_file,int first_gq,int length_gq,
                           int begin_gq,int end_gq) {
    if(!opted_in||is_file||length_gq<=0||begin_gq<0||end_gq<=begin_gq)
        return 0;
    if(first_gq<begin_gq||first_gq>=end_gq)return 0;
    return length_gq<=end_gq-first_gq;
}
