# Uplink FILESERVER 0.7C2 — populated disks based on working 0.7B2 (EXPERIMENTAL)

**Target:** original Steam Windows Uplink 1.55, 32-bit, executable SHA-256:
`837eb6465eb9538018f9b213bbd8d3f385fafd22182626ba6f21858223a34087`.

**What's fixed:** This is built from **0.7B2**, which the user reports now shows free space and supports file copying. It retains the **correct** reverse lookup in the native databank's memory table. The older 0.7C candidate incorrectly called `0x00422F10` as a reverse lookup; **do not use old 0.7C**. No changes to the EXE or the Steam-compatible `VERSION.dll` proxy.

## Changes from 0.7B2

- Newly allocated private/group disks are populated with native Uplink `Data` records (normally **7–12 files requested** per 100 Gq volume, fewer only if disk space is exhausted).
- Uses company prefix names in the spirit of the original LAN generator: e.g. `C&A-data-12345` and `C&A-prog-98765`; randomized sizes, types, encryption, and compression. These file names and values are generated with a local RNG emulating original naming rules, not the game's original NameGenerator functions.
- Each `Data1 = 1` FILESERVER receives its **own** disk and file population. All FILESERVERs with the same `Data1 >= 2` share one group disk and its generated files.
- The root/Main Server and `Data1 = -1/0` FILESERVERs retain their existing shared files.
- Generation starts only **after** all native partition markers are read back with the corrected 0.7B2 reverse lookup and the layout has passed validation. It rechecks the markers after generating files.
- Any volume already containing `UFS2.` guard records is left exactly as it is: no regeneration, migration, or restoration of deleted files. Therefore previously initialized empty 0.7B2 test disks stay empty.
- File copier and deleter use native Uplink DataBank objects/absolute memory indexes, as in 0.7B2.


## Root-only FILESERVER LANs (no extra disk needed)

The Windows 0.7C2 code **already implements** the equivalent of the macOS
root-only fallback fix. A custom LAN containing only FILESERVERs with
`Data1=-1` or `Data1=0` and no UFS2 markers uses the **original native
DataBank and file-row enumerator**. Its main server and root-shared
FILESERVERs see the same generated root files. No partition allocation or
extra private-file generation is supposed to occur in that configuration.
Unexpected UFS2 markers in a root-only layout remain an error, rather than
being bypassed. A static regression guard for both native browsing and
placement exists in `source/tools/test_integration_regressions.py`.

**There is no new Windows DLL required for this fix.** The separate macOS
v0.16.1 bug was not present in the Windows 0.7C2 path; do not overwrite a
working DLL merely to mirror the Mac source change. This remains specific
to the exact Steam Windows **1.55** binary fingerprint shown above.

## `Data1` storage settings

| Data1 | Meaning |
| --- | --- |
| -1 or 0 | Legacy/existing shared Main Server files |
| 1 | Own private populated disk per FILESERVER node |
| 2 or higher | One populated private disk shared among nodes with this group number |

`Data2` and `Data3` unchanged. No new LAN syntax. Each new private/group volume has a capacity of 100 Gq; the root has its original capacity.

## Recommended first Windows test

1. Exit Uplink and **back up the complete `users` folder**, the original C&A LAN definition and working 0.7B2 `UplinkFileServer.dll`. Keep them separate from test saves.
2. **Leave your working `VERSION.dll` and `Uplink.exe` untouched.** Copy only `install/UplinkFileServer.dll` into the same directory as Uplink.exe, replacing the older patch DLL.
3. Make sure both C&A FILESERVER node entries use `Data1 = 1` (don't alter their node indices, coordinates, security, or links). Use the original LAN `.txt` syntax and preserve its other columns.
4. **Start an entirely new disposable agent.** A previously generated LAN embedded in an agent save does *not* reread its `.txt` definition, and 0.7B2's already-created empty partitions will not be filled retroactively.
5. Connect to FILESERVER nodes 4 and 5. Each should show several generated company files **and** remaining free space. They should show different file lists.
6. Copy a small file onto node 4. Confirm it is absent from node 5; then copy or delete one of the generated files and revisit that server.
7. Save the disposable agent, exit and relaunch Uplink. Verify the lists and changes persist without duplication. Separately test `Data1 = 2` on two nodes with another new agent: those nodes should display the **same** group partition/files.
8. If anything fails, upload the **new tail section** of `UplinkFileServer.log` and `debug.log` (if present). Useful lines begin with `=== UplinkFileServer 0.7C2` and `POPULATE:`.

Expected log example (actual counts and indexes vary):

```
=== UplinkFileServer 0.7C2 populated volumes + B2 reverse-index DLL loaded ===
DISKS ACTIVE: four row calls filtered; native automatic placement fenced
DISKS: native partition markers verified immediately after allocation
POPULATE: volume mode 0x00000050
POPULATE: volume key 0x00000004
POPULATE: native files created 0x00000009
POPULATE: first-time generation complete; marker layout recheck
POPULATE: marker layout intact after native file placement
```

`0x50` represents P (private node), `0x47` represents G (group).

## Important risks and limits

- The user has confirmed **0.7B2** free-space and file-copying behavior, **not this 0.7C2 DLL**. This new build was compiled, statically checked against the exact EXE, and tested with portable models only. It has **not** been run in Windows/Uplink here.
- This DLL adds file records to Uplink's real databank and modifies game memory. A crash or corrupted save is possible; never use a valuable agent for the first test.
- Existing 0.7B2 partitions and old 0.7C generated partitions are **not migrated**; generation occurs only when UFS2 markers are absent and the volume is first allocated.
- Global `DataBank::Format`, randomization/relocation, and indirect engine paths bypassing the file browser are not fully audited for partition isolation. Do not format test LANs, and avoid using files with important mission progress.
- The marker layout is saved in the game's native save file. An agent saved after partitioning must not be opened in vanilla Uplink, 0.6, or 0.7A without restoring a **pre-partition save backup**. For rollback, restore 0.7B2 DLL **and** an appropriate clean/save backup if game state misbehaves.
- Generated data uses real native Data records but local randomized names/metadata, not original engine RNG or scripted specific research contents. Content-aware/R&D file customization is a separate future feature.

## Contents

`install/UplinkFileServer.dll` — 0.7C2 patch DLL. 32-bit Windows PE.

`rollback/UplinkFileServer_0.7B2.dll` — user's confirmed working empty-partition DLL.

`rollback/UplinkFileServer_0.7A.dll` and `rollback/UplinkFileServer_0.6.dll` — earlier diagnostic / screen-only versions; use only with compatible pre-partition saves.

`source/src`, `source/tools` — all patch source, Windows and Linux build scripts, source-level static check, original model tests and new regression tests.

`TEST_RESULTS.md` — scope of checks, with what is not tested.

Unofficial experimental mod, not affiliated with Introversion Software.
