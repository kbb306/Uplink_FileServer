#!/bin/bash
# Restore the most recently backed-up dylib created by the v0.16.1 installer.
# NEVER changes game executable or agent saves.
set -euo pipefail
DEST="$HOME/UplinkMods/libUplinkFileServerDiag.dylib"
shopt -s nullglob
BACKUPS=("$DEST".backup-before-v0.16.1-*)
if ((${#BACKUPS[@]} == 0)); then
    echo 'No v0.16.1 installer backup was found. Remove the Steam DYLD_INSERT_LIBRARIES launch option to disable the mod.' >&2
    exit 1
fi
LATEST="$(printf '%s\n' "${BACKUPS[@]}" | LC_ALL=C sort | tail -1)"
if [[ ! -f "$LATEST" ]]; then echo "Backup missing: $LATEST" >&2; exit 1; fi
codesign --verify --strict --verbose=2 "$LATEST"
STAGE_DIR="$(mktemp -d "$HOME/UplinkMods/.ufs-v0161-rollback.XXXXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT
cp -p "$LATEST" "$STAGE_DIR/libUplinkFileServerDiag.dylib"
if [[ -f "$DEST" ]]; then
    cp -p "$DEST" "$DEST.v0161-disabled-$(date +%Y%m%d-%H%M%S)-$$"
fi
mv -f "$STAGE_DIR/libUplinkFileServerDiag.dylib" "$DEST"
codesign --verify --strict --verbose=2 "$DEST"
echo "Restored previous signed library from: $LATEST"
echo 'Save files were not changed by this rollback.'
