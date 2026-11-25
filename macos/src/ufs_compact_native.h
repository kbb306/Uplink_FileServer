#ifndef UFS_COMPACT_NATIVE_H
#define UFS_COMPACT_NATIVE_H
#include "browser_route.h"
#include "ufs_compact.h"
/* Caller must pass a fully validated UFS2 view from this same screen. */
int ufs_compact_native(void *computer, const struct ufs_browser_view *view,
                       struct ufs_compact_result *out);
#endif
