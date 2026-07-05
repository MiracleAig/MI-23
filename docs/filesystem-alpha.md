# MI-23 Filesystem Alpha

MI-23 Filesystem Alpha builds on AxiomFS, the shared filesystem facade used by
host simulator builds and RP2350 firmware builds.

Default directory layout:

```text
/
  settings/
  programs/
  graphs/
  notes/
  themes/
  logs/
  cache/
  screenshots/
```

Startup runs an AxiomFS health check. It mounts storage, creates missing default
folders, writes/reads/deletes a temporary probe file in `cache/`, and stores the
last result in memory for Settings. If mount fails, startup does not format
storage automatically.

The Settings app includes a Storage Manager page. It shows filesystem status,
backend type, total/used/free storage, and file count when available. It can
remount, rerun the health check, or format storage after a confirmation screen.

Host files are stored in `./mi23_fs/` by default. RP2350 builds reserve the top
512 KiB of flash for LittleFS; builds without vendored LittleFS use a safe stub.

Currently persistent:

- Calculator history in `cache/history.json`
- Boot and filesystem log lines in `logs/boot.log` and `logs/fs.log`
- Graph sessions in readable `graphs/*.mi23graph` files

Known limitations:

- File Browser supports browse, open folder, details, go up, and confirmed file
  delete. Rename and create-folder UI are planned after text-entry dialogs exist.
- File Browser shows graph session files, but direct launch into Graphing remains
  planned.
- Settings still use the existing `SettingsStore`; migration to AxiomFS remains
  a follow-up.
- The desktop companion app is intentionally out of scope for this milestone.

Planned companion support will use the same directory layout for transferred
programs, notes, themes, and screenshots.

Release note: Graph sessions can now be saved and loaded from persistent
storage.
