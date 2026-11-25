/* UFS2 v0.10: Only allow selecting a free-space row as an upload target.
 * All existing file rows are blocked from native FileClick routing.
 * v0.10 private safe deletion is a separate, explicitly gated path.
 */
#ifndef UFS_CLICK_POLICY_H
#define UFS_CLICK_POLICY_H
int ufs_click_policy_allow(int opted_in,int is_file,int first_gq,int length_gq,
                           int begin_gq,int end_gq);
#endif
