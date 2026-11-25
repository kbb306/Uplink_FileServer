# Uplink FILESERVER

Unofficial experimental mods restoring type-7 `FILESERVER` nodes in custom LANs for Uplink.

| Platform | Target | Mod | Documentation |
| --- | --- | --- | --- |
| Windows (32-bit) | Steam 1.55 | 0.7C2 | [Windows README](windows/README.md) |
| macOS (Intel x86_64) | Steam 1.6.9 | 0.16.1 | [macOS README](macos/README.md) |

Each platform directory preserves the original source, build and installation files. Back up saves before use; these patches target exact executables. A legitimate copy of Uplink is required.

## DATA1 storage modes

- `-1` or `0`: shared Main Server/root storage.
- `1`: separate private disk per FILESERVER.
- `2` and higher: one shared disk per numeric group.

Private and grouped disks have 100 Gq capacity. See the platform documentation for limitations and migration instructions.

The macOS package includes its original MIT license; no new license has been assigned to the Windows package.
