#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd -P)"
ROOT="$(cd "$HERE/.." && pwd -P)"
TEMP="$(mktemp -d "${TMPDIR:-/tmp}/ufs-mac-v0161-tests.XXXXXXXX")"
trap 'rm -rf "$TEMP"' EXIT
CLANG="$(command -v clang || true)"
if [[ -z "$CLANG" ]]; then echo 'clang is required' >&2; exit 1; fi
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_ufs_compact.c" -o "$TEMP/test_ufs_compact"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_operation_policy.c" "$ROOT/src/ufs_operation_policy.c" -o "$TEMP/test_operation_policy"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_private_population.c" \
  "$ROOT/src/private_population.c" "$ROOT/src/active_layout.c" \
  "$ROOT/src/partition_commit.c" "$ROOT/src/native_bank.c" "$ROOT/src/volume_plan.c" \
  -o "$TEMP/test_private_population"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_grouped_lan.c" \
  "$ROOT/src/private_population.c" "$ROOT/src/browser_route.c" \
  "$ROOT/src/safe_delete.c" "$ROOT/src/ufs_compact_native.c" \
  "$ROOT/src/active_layout.c" "$ROOT/src/partition_commit.c" \
  "$ROOT/src/native_bank.c" "$ROOT/src/volume_plan.c" \
  -o "$TEMP/test_grouped_lan"
"$TEMP/test_grouped_lan"
"$TEMP/test_private_population"
"$TEMP/test_operation_policy"
"$TEMP/test_ufs_compact"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_vanilla_remote_repack.c" -o "$TEMP/test_vanilla_remote_repack"
"$TEMP/test_vanilla_remote_repack"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_volume_plan.c" "$ROOT/src/volume_plan.c" -o "$TEMP/test_volumes"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_native_bank.c" "$ROOT/src/native_bank.c" "$ROOT/src/volume_plan.c" -o "$TEMP/test_native_bank"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_volume_rows.c" "$ROOT/src/volume_rows.c" -o "$TEMP/test_rows"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_layout.c" -o "$TEMP/test_layout"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_postload_guard.c" "$ROOT/src/postload_guard.c" -o "$TEMP/test_postload_guard"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_partition_commit.c" "$ROOT/src/partition_commit.c" "$ROOT/src/native_bank.c" "$ROOT/src/volume_plan.c" -o "$TEMP/test_partition_commit"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_browser_route.c" "$ROOT/src/browser_route.c" "$ROOT/src/active_layout.c" "$ROOT/src/partition_commit.c" "$ROOT/src/native_bank.c" "$ROOT/src/volume_plan.c" -o "$TEMP/test_browser_route"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_upload_guard.c" "$ROOT/src/volume_plan.c" -o "$TEMP/test_upload_guard"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_click_policy.c" "$ROOT/src/click_policy.c" -o "$TEMP/test_click_policy"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_safe_delete.c" \
  "$ROOT/src/safe_delete.c" "$ROOT/src/ufs_compact_native.c" "$ROOT/src/browser_route.c" "$ROOT/src/active_layout.c" \
  "$ROOT/src/partition_commit.c" "$ROOT/src/native_bank.c" "$ROOT/src/volume_plan.c" \
  -o "$TEMP/test_safe_delete"
"$CLANG" -std=c11 -O2 -Wall -Wextra -Werror "$HERE/test_vanilla_delete_timing.c" "$ROOT/src/vanilla_delete_timing.c" -o "$TEMP/test_vanilla_delete_timing"
"$TEMP/test_vanilla_delete_timing"
"$TEMP/test_safe_delete"
"$TEMP/test_click_policy"
"$TEMP/test_upload_guard"
"$TEMP/test_browser_route"
"$TEMP/test_volumes"
"$TEMP/test_native_bank"
"$TEMP/test_rows"
"$TEMP/test_layout"
"$TEMP/test_postload_guard"
"$TEMP/test_partition_commit"
python3 "$HERE/test_save_probe.py"
