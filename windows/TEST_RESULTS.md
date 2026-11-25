# Verification report: Uplink FILESERVER 0.7C2

## Origin

0.7C2 uses source of **0.7B2** as the base, including the fixed `file_slot_to_memory_index()` implementation that scans the native memory DArray (`DataBank +0x1C` array, `+0x20` validity flags). `0x00422F10` is NOT used as a reverse lookup.

The population generator originally prototyped in old 0.7C was transplanted to B2 and moved to run *after* B2 has verified guard file positions and the partition layout. Guard verification runs again after populating. `GetTickCount` is the only additional Win32 API import.

## Passed offline checks

- Both supplied executable copies have the exact expected SHA-256, `837eb6465eb9538018f9b213bbd8d3f385fafd22182626ba6f21858223a34087`.
- Compiles as PE32 Intel 80386 Windows DLL with Clang targeting `i686-pc-windows-msvc` and `lld-link`, with `-Wall -Wextra -Werror`.
- `test_disk_model.py`: partition mapping, group modes, free-space clipping and simulated save roundtrip.
- `test_reverse_index.py`: correct file-index -> memory-index mapping including four guard slots matching earlier user logs.
- `test_population_model.py`: 3,000 random seed runs verify generated record boundaries, prefixes, types, encryption and compression, per-node/private vs grouped keys, modeled persistence.
- `test_integration_regressions.py`: checks that wrong `n_get_mem_index` pointer is absent, correct reverse lookup is in use, generation is gated by marker/layout verification, saved partitions cannot trigger new population and native DataBank placement is used.
- `verify_07b_static.py`: PE signature, x86 DLL, exact native code fingerprints and four row scanner call sites.
- `verify_population_static.py`: exact EXE, company-name and Data constructor field offsets, native code signatures, GetTickCount import and embedded POPULATE logs.

## Not passed / not yet tested

- Actual startup in Windows/Steam.
- The real game displaying populated files and residual free space.
- Actual file copying, deletion, encryption, compression and independent/grouped visibility in this new version (though the user confirmed copies with 0.7B2).
- Native save/reload and game world missions, especially formatting and indirect file operations.

Use a fresh, disposable agent and retain the 0.7B2 DLL and user save backups.
