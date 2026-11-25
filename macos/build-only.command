#!/bin/bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd -P)"
cd "$HERE"
if [[ "$(uname -s)" != Darwin ]]; then echo 'ERROR: Run this on the Intel Mac.' >&2; exit 1; fi
if ! xcrun --find clang >/dev/null 2>&1; then echo 'Install Apple Command Line Tools (xcode-select --install).' >&2; exit 1; fi
mkdir -p build
xcrun --sdk macosx clang -arch x86_64 -mmacosx-version-min=10.13 \
    -dynamiclib -O2 -fPIC -fno-builtin -fno-omit-frame-pointer \
    -Wall -Wextra -Werror \
    -Wl,-install_name,@rpath/libUplinkFileServerDiag.dylib \
    src/ufs_mac.c src/private_population.c src/ufs_operation_policy.c src/ufs_compact_native.c src/vanilla_delete_timing.c src/safe_delete.c src/volume_plan.c src/native_bank.c src/volume_rows.c src/postload_guard.c src/partition_commit.c src/active_layout.c src/browser_route.c src/click_policy.c -o build/libUplinkFileServerDiag.dylib
codesign --force --sign - --timestamp=none build/libUplinkFileServerDiag.dylib
codesign --verify --strict --verbose=2 build/libUplinkFileServerDiag.dylib
lipo -info build/libUplinkFileServerDiag.dylib
echo 'Mac v0.16.1 FILESERVER dylib compiled and signed.'
