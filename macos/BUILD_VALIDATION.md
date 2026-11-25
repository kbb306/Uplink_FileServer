# Mac FILESERVER v0.16.1 offline validation

- Exact target: Intel `x86_64` Uplink Steam for Mac 1.6.9, LC_UUID `be676540-d8cc-326d-8e69-8a25e57cd10d`.
- No binary patching or changed Uplink executable. Install uses signed `DYLD_INSERT_LIBRARIES` dylib built on Mac.
- Architecture: DArray based DATA1 plan reconstructed from native LAN systems; shared root R (`-1`, `0`), private P (`1`), grouped G (`2+`). Marker count is `2 * distinct private/group volumes`, not `2 * FILESERVER nodes`.
- Per-physical-volume generation uses verified native `DataBank::PutData` and a hidden `Data::version=57` marker stamp to prevent re-generation on load, deletion or across group members.
- Guard checks: native LAN computer type, plan match for fresh LoadLAN, native bank sanity, exact marker pairs, all contiguous file extents, region confinement, guarded FileClick/screen identity, no native destructive File_Deleter terminal path.
- New saved-LAN migration is an explicit opt-in `UPLINK_UFS16_MIGRATE_SAVED=1`; without it, markerless already-saved LANs retain vanilla browser routing. Corrupt/partially UFS2-marked banks fail closed.
- Tests: `bash tests/run-model-tests.sh` — 16 C model suites plus REDSHRT2 checksum fixture probe passed. New `test_grouped_lan.c` exercises the grouped `DATA1=2` storage, independent `DATA1=1`, shared root, root file preservation, file generation idempotence, native-screen mapping, scoped File_Deleter removal, repacking and corruption handling. Includes compiler `-Wall -Wextra -Werror`.
- All 14 source `.c` files cross-compiled to Intel Mach-O `.o` with `clang -target x86_64-apple-macos10.13 -std=c11 -O2 -fPIC -fno-builtin -fno-omit-frame-pointer -Wall -Wextra -Werror`.
- Real Marbles save previously uploaded `Marbles(20261125-071710).usr` still validates: checksum PASS, one native ENCOM UFS2 bank, 396 Gq, 38 files, six markers, contiguous extents and per-disk confinement PASS.
- Not done: running the new code inside macOS Uplink; signing/linking a dylib without macOS SDK; live testing the group LAN or on-disk migrated save. All remain for controlled user-side testing.

- v0.16.1 bugfix: File_Deleter animation arm uses the same P/R/G admission predicate as the click policy, with regression tests for P, R, G and rejected unknown modes. Only admission of already-validated G partitions changed; all native task and bank guards are retained.
