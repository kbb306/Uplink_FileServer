#include "../src/click_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    /* Even opted in, a FILE row is never passed to native FileDeleter. */
    assert(!ufs_click_policy_allow(1,1,91,1,91,191));
    assert(!ufs_click_policy_allow(1,1,0,6,0,90));
    /* Upload destinations remain selectable within strictly guarded bounds. */
    assert(ufs_click_policy_allow(1,0,92,99,91,191));
    assert(ufs_click_policy_allow(1,0,91,100,91,191));
    assert(ufs_click_policy_allow(1,0,59,31,0,90));
    assert(!ufs_click_policy_allow(0,0,91,100,91,191));
    assert(!ufs_click_policy_allow(1,0,90,1,91,191));
    assert(!ufs_click_policy_allow(1,0,190,2,91,191));
    assert(!ufs_click_policy_allow(1,0,191,1,91,191));
    assert(!ufs_click_policy_allow(1,0,91,0,91,191));
    assert(!ufs_click_policy_allow(1,0,91,-1,91,191));
    puts("v0.11 click policy: PASS (file rows blocked, free rows gated)");
    return 0;
}
