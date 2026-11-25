# Uplink FILESERVER — Mac Steam 1.6.9 v0.16.1 (experimental)

## v0.16.1: Grouped File_Deleter bug fix

The v0.16 grouped (`DATA1>=2`, mode `G`) file browser correctly routed uploads,
downloads and clicks, but the final native File_Deleter animation arm retained
an older P/R-only admission check. It rejected every attempted delete from
mode G with `ANIMATION_ARM_REJECTED (native target not verified)` even when the
native tool was selected correctly. v0.16.1 now shares the same P/R/G mode
admission predicate with the click policy. The native task identity, vtable,
marker, file-extent, screen, CPU timing and scoped delete/repack checks are
otherwise unchanged. No migration or new file generation is needed for an
already valid v0.16 save. **Remove `UPLINK_UFS16_MIGRATE_SAVED=1` from your
Steam launch option once your test LAN has been migrated.**

### Upgrade in place

Quit Uplink, back up your test agent, extract to
`~/Downloads/UplinkFileServerMac_0.16.1`, then run:

```sh
cd ~/Downloads/UplinkFileServerMac_0.16.1
bash ./install-fileserver.command
```

Steam must still inject `~/UplinkMods/libUplinkFileServerDiag.dylib` and set
`UPLINK_UFS09_OPERATIONS=1` plus `UPLINK_UFS10_SAFE_DELETE=1`. Save the
newly migrated test agent only when validation succeeds. For one-time rollback
of the dylib use `bash ./rollback-fileserver.command` from this version's
directory. Removing a dylib does not reverse previously migrated saves.

On the Valve test LAN, choose a **disposable** file in a `DATA1=2` or `DATA1=3`
server, activate File_Deleter, and click it. The log should now show
`v0.16.1 ANIMATION_ARMED ...` (the arm message records node/Gq;
verify `v0.16 BROWSER_ROUTE ... mode=G` on the same screen). Completion should
log `v0.14 ANIMATED_SAFE_DELETE=PASS ...` and
`v0.14 VANILLA_REMOTE_REPACK=PASS`. Both members of that group should no
longer show the deleted file, while the opposite group is unchanged.

## What changed in v0.16

v0.16 removes the hard-coded ENCOM R&D Network identity and four-node restrictions from v0.15. It supports **custom LANs** that contain type-7 `FILESERVER` nodes, for the exact Intel Mac Uplink 1.6.9 executable previously tested (LC_UUID `be676540-d8cc-326d-8e69-8a25e57cd10d`). It does not make a cross-version binary patch. The game executable remains unchanged.

**DATA1 disk semantics** (DATA2/DATA3 unchanged):

| DATA1 | Interpretation | Files | Allocation |
| --- | --- | --- | --- |
| `-1` | Legacy shared root | All root-shared FILESERVERs and the normal LAN Main Server see the same original root bank | No added partition |
| `0` | Shared Main Server/root | Same original root | No added partition |
| `1` | Node-private | Each FILESERVER gets an independent 100-Gq disk | One pair of hidden UFS2 P markers per node |
| `2` and greater | Grouped | Every FILESERVER with the **same numeric DATA1** sees one and the same 100-Gq disk, with identical file additions/deletions | One pair of hidden UFS2 G markers per distinct group number |

`DATA1 < -1` is rejected. No physical disk is created twice for a shared group. Group membership is reconstructed from the saved LAN's actual system `DATA1` values, not from a fixed list of ENCOM node indices. The number of marker pairs is dynamic (not always six).

Initial file generation is performed **once per physical private or group disk**, using ordinary native `Data` objects and the original game's `DataBank::PutData` operation; existing files are preserved. Hidden BEGIN marker `Data::version=57.0` persists the generated state, so deleted files do not reappear on reload. Generated files use a company-prefix `-data-` or `-prog-` naming scheme, sizes, optional encryption and compression. Occupancy is capped at approximately 75 Gq of the 100 Gq, preserving upload room.

The existing v0.14-v0.15 root/private browser mapping, File_Copier routing, opt-in native-CPU-timed safe File_Deleter, and bounded repacking now also accept grouped (`G`) disks. All file operations still require full bank/marker/node/screen checks. On a bad layout, the mod fails closed rather than allowing a cross-disk deletion.

## Install

**Quit Uplink first. Back up your agent's `.usr` save and use a disposable test agent.** Partition creation and automatic file generation mutate the save via the *game* and are persisted when you save. The installer itself does not edit Uplink.app or your save.

Extract into `~/Downloads/UplinkFileServerMac_0.16.1` and run:

```sh
cd ~/Downloads/UplinkFileServerMac_0.16.1
bash ./install-fileserver.command
```

This builds `x86_64` using Apple's `xcrun clang`, ad-hoc signs the dylib, backs up the existing installed library to a timestamped `backup-before-v0.16.1-*`, and atomically installs `~/UplinkMods/libUplinkFileServerDiag.dylib`. Leave the **already working Steam launch option** pointing to that dylib unchanged. It should continue passing `UPLINK_UFS09_OPERATIONS=1` and `UPLINK_UFS10_SAFE_DELETE=1` for uploads and File_Deleter.

If the custom LAN is generated *for the first time* after installing v0.16, its disks are partitioned and generated automatically during LoadLAN. If you **already visited/generated** that LAN in an older saved agent, it will have no UFS2 boundaries. To convert that *existing* LAN, add **temporarily** the following argument before `--env DYLD_INSERT_LIBRARIES` in your current Steam launch option:

```
--env UPLINK_UFS16_MIGRATE_SAVED=1
```

Only perform this with a backed-up, disposable save. On the first file-browser draw of each saved, markerless custom LAN with an otherwise valid type-7 layout, the mod will allocate partitions, populate each private/group disk, and log `v0.16 SAVED_MIGRATION`. **Remove the migration argument after your intended test LAN is converted** to avoid inadvertently converting additional existing LANs. Saved LANs with no type-7 private/group volumes continue with normal native behavior when migration is disabled. This is not a tool for repairing corrupted or partially partitioned LANs: invalid UFS2 layouts remain blocked.

To suppress *generation* without disabling partitioning, use the pre-existing `--env UPLINK_UFS15_GENERATE=0` argument.

## First test: group sharing

On a new disposable agent, use a LAN that has two FILESERVER nodes with `DATA1=2`, and preferably one additional node with `DATA1=1`. The grouped nodes should show precisely the same file list and free space, while the private node should show its own files. Upload a disposable file to the first group node, open the other group node and verify that it appears; then delete it on the second group node and verify that it disappears from the first. The delete should use native CPU-timed File_Deleter and not affect the root or any other group's files.

Save, quit, reload and revisit both group members. Files should persist without duplication. For an already-visited LAN, use the one-time migration opt-in above. If a browser first appears blank immediately after new generation, close/reopen it once. In case of any `ALLOCATE_RESULT=FAIL`, `SAVED_MIGRATION` with negative result/stage>0, or `PRIVATE_GENERATION` with a negative result, **discard the disposable agent save** rather than saving the partial bank.

Inspect the log:

```sh
tail -n 100 "$HOME/Library/Logs/UplinkFileServer-Mac-Diagnostic.log"
```

Look for `v0.16 DATA1_PLAN ... mode=G key=2`, `ALLOCATE_RESULT=PASS`, `PRIVATE_GENERATION`, and `v0.16 BROWSER_ROUTE ... mode=G ... range=[...,... )`. Both group members should have identical ranges and a **single** shared UFS2 boundary pair `UFS2.B.G00000002` / `UFS2.E.G00000002`. The exact Gq range depends on the preceding disks and root size.

The included **read-only**, generic verifier supports different marker counts and P/G disks:

```sh
python3 ~/Downloads/UplinkFileServerMac_0.16.1/tools/verify_ufs2_any_save.py \
  "/path/to/disposable-agent.usr"
```

The older `verify_saved_ufs2_active.py` remains ENCOM-specific and is included only to compare existing ENCOM saves; do **not** use it to validate a new grouped LAN.

## Rollback

Quit Uplink and run:

```sh
cd ~/Downloads/UplinkFileServerMac_0.16.1
bash ./rollback-fileserver.command
```

The most recent **v0.16.1 installer** backup will be restored and verified with `codesign`. This does not undo save changes, so restore your pre-test `.usr` backup too if reverting the test.

## Verification and limitations

The package contains 14 Intel `x86_64` Mach-O C object files, all sources, installation and rollback scripts, 16 offline C model suites plus a save-format probe, and validators. The tests include a non-ENCOM mock LAN with two `DATA1=2` nodes sharing one disk, a `DATA1=1` private disk, and a root Main Server. They cover group file-generation idempotence, scoped deletion and repack, saved-LAN plan reconstruction, and corruption rejection. The source cross-compiles with strict warnings-as-errors. An actual `.dylib` must still be linked and signed **on your Mac**; we cannot run Uplink 1.6.9 in this environment.

v0.16 group separation/upload was live-tested on your Valve LAN, but v0.16.1 grouped deletion has **not yet** been live-tested. Do not overwrite your only good agent save. The mod still targets **only the exact Mac executable build**, and it does not implement remote Defrag, new game features outside custom LAN FILESERVERs, or a general-purpose repair tool.
