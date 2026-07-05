# AxiomFS

AxiomFS is the shared MI-23 filesystem abstraction. The public API lives in
`firmware/hal/fs/axiom_fs.h` and exposes mount, format, exists, read, write,
delete, directory listing, and space queries through a small backend interface.

Host builds use `HostAxiomFSBackend`, which stores simulator files in
`./mi23_fs/` by default and creates the folder on mount. Paths are normalized
before use: absolute paths, backslashes, and `..` components are rejected so
simulator code cannot escape the storage root.

RP2350 builds reserve the top 512 KiB of flash for LittleFS. The flash block
device uses RP2350 flash sectors as LittleFS erase blocks and flash pages as
program units. The existing raw settings sector is placed immediately before
the AxiomFS region until settings persistence is migrated to files.

Boot code mounts AxiomFS and reports failures, but it does not format
automatically. Formatting is only available through the explicit
`repairOrFormatForDevMode()` development hook or a direct `format()` call.

LittleFS source is not vendored in this repository. RP2350 builds compile a
safe stub by default; enable the real LittleFS backend with:

```bash
cmake -S . -B build-rp2350 -DPLATFORM=rp2350 \
  -DMI23_ENABLE_LITTLEFS=ON \
  -DMI23_LITTLEFS_SOURCE_DIR=/path/to/littlefs
```

The LittleFS source directory must contain `lfs.h`, `lfs.c`, and `lfs_util.c`.

TODO: migrate `SettingsStore` to read and write an AxiomFS file once boot
settings recovery can be switched without changing existing startup behavior.
