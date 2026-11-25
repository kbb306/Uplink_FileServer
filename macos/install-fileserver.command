#!/bin/bash
# Never overwrite signed code in place (avoids macOS Code Signature Invalid crash).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd -P)"
bash "$HERE/build-only.command"
MOD_DIR="$HOME/UplinkMods"
DEST="$MOD_DIR/libUplinkFileServerDiag.dylib"
SOURCE="$HERE/build/libUplinkFileServerDiag.dylib"
mkdir -p "$MOD_DIR"
STAGE_DIR="$(mktemp -d "$MOD_DIR/.ufs-v0161-stage.XXXXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT
STAGED="$STAGE_DIR/libUplinkFileServerDiag.dylib"
cp -p "$SOURCE" "$STAGED"
codesign --verify --strict --verbose=2 "$STAGED"
if [[ "$(shasum -a 256 "$SOURCE" | awk '{print $1}')" != "$(shasum -a 256 "$STAGED" | awk '{print $1}')" ]]; then
   echo 'Staged signed library differs from build; refusing installation.' >&2
   exit 1
fi
if [[ -e "$DEST" ]]; then
  BACKUP="$DEST.backup-before-v0.16.1-$(date +%Y%m%d-%H%M%S)-$$"
  cp -p "$DEST" "$BACKUP"
  echo "Previous dylib backed up: $BACKUP"
fi
mv -f "$STAGED" "$DEST"
codesign --verify --strict --verbose=2 "$DEST"
echo "Installed Mac FILESERVER v0.16.1: $DEST"
echo 'Existing Steam launch option pointing at libUplinkFileServerDiag.dylib should remain unchanged.'
echo 'TEST WITH A COPY OF THE VALIDATED PARTITIONED SAVE, NOT YOUR MAIN AGENT.'
