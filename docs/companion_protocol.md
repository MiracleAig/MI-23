# MI-23 Companion Protocol

MI-23 companion protocol v1 is a line-oriented JSON protocol over USB CDC serial.
Each request is one JSON object followed by `\n`; each response is one JSON object
followed by `\n`.

## Build and Flash RP2350

Build the firmware:

```bash
./build.sh --platform=rp2350
```

The build produces:

```text
build-rp2350/firmware/platform/rp2350/mi23.uf2
```

To flash a Waveshare RP2350-PiZero board, put the board in BOOTSEL mode and copy
the UF2 file to the mounted RP2350 boot volume.

## Connect on Linux

After the device boots, the USB CDC serial port should appear as a device such
as `/dev/ttyACM0`.

Example with `picocom`:

```bash
picocom -b 115200 /dev/ttyACM0
```

The baud rate is not meaningful for USB CDC, but most serial tools require one.
Send one compact JSON object per line.

## Response Format

Success responses:

```json
{"id":1,"ok":true,"result":{}}
```

Error responses:

```json
{"id":1,"ok":false,"error":{"code":"unknown_command","message":"Unknown command"}}
```

Every valid response echoes the request `id`. If the request cannot be parsed or
does not contain an integer `id`, firmware uses `id:0`.

## Discovery

Device info request. `device.info` is the canonical v1 JSON command name.
`GET_DEVICE_INFO` is accepted as a compatibility alias for companion clients
that ask for the command in uppercase token form.

```json
{"id":1,"cmd":"device.info"}
```

Equivalent alias:

```json
{"id":1,"cmd":"GET_DEVICE_INFO"}
```

Expected RP2350 response fields include:

```json
{"id":1,"ok":true,"result":{"model":"MI-23","name":"MI-23","firmware":"0.1.0-alpha.1","protocol":1,"transport":"usb_cdc","serial":"mi23-0123456789ABCDEF","hardware_revision":"waveshare-rp2350-pizero","product_id":"mi23","product_name":"MI-23","device_id":"mi23-0123456789ABCDEF","firmware_version":"0.1.0-alpha.1","protocol_version":1,"filesystem_schema_version":1,"platform":"rp2350","flash_size_bytes":16777216,"filesystem_offset_bytes":14680064,"filesystem_size_bytes":2097152,"supports_bootsel_reboot":true,"supports_firmware_update":true,"supports_file_transfer":true,"supports_filesystem_backup":true}}
```

The legacy `model`, `name`, `firmware`, and `protocol` fields remain for
existing clients. New clients should prefer `product_id`, `product_name`,
`firmware_version`, and `protocol_version`.

`device_id` is a stable, non-secret reconnect identifier. RP2350 firmware uses
the Pico SDK unique board identifier and prefixes it with `mi23-`. The host
simulator reports `mi23-host-simulator`.

Current exact `hardware_revision` strings:

- `waveshare-rp2350-pizero`: Waveshare RP2350-PiZero prototype firmware target.
- `host-simulator`: desktop simulator build.

`platform` is `rp2350` on hardware and `host` in the simulator.

There is no version-negotiation request in companion protocol v1. The
`protocol_version` field reports the implemented protocol revision for the
current connection. Clients should treat version `1` as the current line-oriented
JSON protocol documented here and reject or fall back for unsupported future
major protocol versions.

`filesystem_schema_version` reports the firmware's AxiomFS/LittleFS on-device
layout/schema version. The current schema version is `1`; it should only be
incremented when an update changes persisted storage layout or serialized file
schemas in a way that companion software must handle explicitly.

Capability fields:

- `supports_file_transfer`: `fs.list`, `fs.read`, `fs.write`, `fs.mkdir`, and
  `fs.delete` are available.
- `supports_filesystem_backup`: the filesystem can be enumerated and read by
  the companion app for backup.
- `supports_firmware_update`: the firmware is eligible for companion-managed
  update/reinstall/downgrade flows. The calculator does not download firmware
  from the network and does not flash itself.
- `supports_bootsel_reboot`: the platform can reboot into RP2350 BOOTSEL/USB
  BOOT mode through the protocol. RP2350 reports `true`; host metadata reports
  `false`, though the simulator has a safe mock for development.

Flash geometry fields are reported so the companion can avoid installing a
firmware image that would overwrite user storage:

- `flash_size_bytes`: `16777216` on the Waveshare RP2350-PiZero target.
- `filesystem_offset_bytes`: `14680064` (`0x00E00000`) on RP2350.
- `filesystem_size_bytes`: `2097152` on RP2350.

The public firmware server is:

```text
https://miraclesinstruments-firmware.duckdns.org
```

The companion app should match `product_id`, `hardware_revision`, and compatible
`protocol_version` before offering firmware from that server. For downgrades,
compare the target firmware's filesystem schema support with the device's
`filesystem_schema_version` and warn before installing older firmware that may
not understand newer stored data.

Capabilities request:

```json
{"id":2,"cmd":"device.capabilities"}
```

The v1 firmware advertises implemented firmware-side handlers:

```json
{"filesystem":true,"settings":true,"terminal":true,"graphs":true,"screenshots":false,"battery":false,"firmware_update":false}
```

`firmware_update` remains `false` here because there are no firmware flashing
commands in v1. Use `device.info`/`GET_DEVICE_INFO` for update eligibility and
exact hardware matching.

Ping:

```json
{"id":3,"cmd":"protocol.ping"}
```

## BOOTSEL Reboot

`device.enter_bootloader` is the canonical v1 command for entering RP2350
BOOTSEL mode. `ENTER_BOOTLOADER` is accepted as an uppercase compatibility
alias.

Request:

```json
{"id":4,"cmd":"device.enter_bootloader"}
```

Equivalent alias:

```json
{"id":4,"cmd":"ENTER_BOOTLOADER"}
```

Success acknowledgement:

```json
{"id":4,"ok":true,"result":{"accepted":true,"mode":"bootsel","reboot_scheduled":true}}
```

After sending the acknowledgement, RP2350 firmware defers the reset briefly so
the USB CDC response can leave the device, flushes stdio, deinitializes normal
USB stdio, and reboots into BOOTSEL using the Pico SDK boot ROM reboot path.
The companion should expect the CDC serial connection to disconnect and the
RP2350 BOOTSEL device to enumerate.

Safety sequence before acknowledgement:

1. Reject the request with `busy` if a file read/write/list/delete/mkdir or
   storage format operation is active.
2. Synchronize AxiomFS/LittleFS.
3. Save current settings through the platform settings store.
4. Schedule the platform BOOTSEL reboot hook.
5. Return the success acknowledgement above.

Repeated BOOTSEL requests while the reboot is already pending are idempotent and
may include:

```json
{"id":5,"ok":true,"result":{"accepted":true,"mode":"bootsel","reboot_scheduled":true,"already_pending":true}}
```

Unsupported platforms return:

```json
{"id":4,"ok":false,"error":{"code":"unsupported","message":"BOOTSEL reboot is not supported on this platform."}}
```

Unsafe states return:

```json
{"id":4,"ok":false,"error":{"code":"busy","message":"Cannot enter BOOTSEL while file upload is active"}}
```

## Storage

Storage status:

```json
{"id":6,"cmd":"storage.info"}
```

Formatting storage requires the exact confirmation string:

```json
{"id":7,"cmd":"storage.format","confirm":"FORMAT_MI23_STORAGE"}
```

The protocol never formats storage automatically. `ENTER_BOOTLOADER` and
`device.enter_bootloader` never erase flash and never format LittleFS.

## Filesystem

Paths are absolute virtual paths inside calculator storage. They must begin with
`/`, are limited in length, and reject traversal such as `..`.

List the root directory:

```json
{"id":6,"cmd":"fs.list","path":"/"}
```

Write a small text file:

```json
{"id":7,"cmd":"fs.write","path":"/test.txt","offset":0,"data_b64":"SGVsbG8=","truncate":true}
```

Read it back:

```json
{"id":8,"cmd":"fs.read","path":"/test.txt","offset":0,"length":512}
```

Create a directory:

```json
{"id":9,"cmd":"fs.mkdir","path":"/notes/demo"}
```

Delete a file:

```json
{"id":10,"cmd":"fs.delete","path":"/test.txt"}
```

File transfer is chunked. Current firmware accepts request lines up to 2048
bytes and file payload chunks up to 1024 decoded bytes.

## Settings

Get safe user settings:

```json
{"id":11,"cmd":"settings.get"}
```

Set one or more safe settings:

```json
{"id":12,"cmd":"settings.set","values":{"angle_mode":"deg","theme":"light"}}
```

Supported editable keys are `angle_mode`, `theme`, `graph_grid`, `graph_axes`,
`graph_resolution`, `ui_scale`, and `calculator_precision`.

## Terminal

The terminal command interface is a small dispatcher, not a shell.

```json
{"id":13,"cmd":"terminal.exec","line":"help"}
```

Supported lines are `help`, `info`, `storage`, `capabilities`, `uptime`,
`version`, `reboot`, and `bootloader`.

`reboot` requests a normal firmware reboot. `bootloader` requests USB BOOT mode
when the platform supports it. Unsupported platforms return an `unsupported`
error instead of attempting a reset.

## Graphs

List saved graph session files:

```json
{"id":14,"cmd":"graphs.list"}
```

Graph files are currently saved graph sessions under `/graphs` with the
`.mi23graph` extension. Graph preview/rendering is not advertised by this
protocol.

## Release Metadata

Firmware, device, protocol, filesystem, and capability metadata are configured
in the top-level CMake project and generated into `mi23_metadata.h` at build
time. The public firmware version is not stored in CMake's numeric
`project(VERSION)` field, so prerelease strings such as `0.1.0-alpha.1` are
valid.

To change the firmware version for a release:

1. Configure a clean release build or update the CMake cache with the new
   semantic version string.
2. Build host/tests first.
3. Build RP2350 and confirm the post-link LittleFS boundary check passes.
4. Publish firmware metadata on the companion firmware server using the same
   `product_id`, `hardware_revision`, `firmware_version`,
   `filesystem_schema_version`, and flash geometry values reported by firmware.

Example:

```bash
cmake -S . -B build-rp2350-release -DPLATFORM=rp2350 -DMI23_FIRMWARE_VERSION=0.1.0-alpha.2
```

Then build normally:

```bash
cmake --build build-rp2350-release --parallel "$(nproc)"
```

The current metadata knobs are:

- `MI23_PRODUCT_ID` default `mi23`
- `MI23_PRODUCT_NAME` default `MI-23`
- `MI23_FIRMWARE_VERSION` default `0.1.0-alpha.1`
- `MI23_HARDWARE_REVISION` default `waveshare-rp2350-pizero` on RP2350 and `host-simulator` on host
- `MI23_COMPANION_PROTOCOL_VERSION` default `1`
- `MI23_FILESYSTEM_SCHEMA_VERSION` default `1`
- `MI23_FLASH_SIZE_BYTES` default `16777216` on RP2350
- `MI23_FILESYSTEM_OFFSET_BYTES` default `14680064` on RP2350
- `MI23_FILESYSTEM_SIZE_BYTES` default `2097152` on RP2350

Increment `MI23_FILESYSTEM_SCHEMA_VERSION` only when persisted filesystem data
or serialized file formats change in a way that older firmware or companion
software must handle explicitly. Do not increment it for ordinary firmware
bugfixes that preserve the existing on-device data layout.

To add a future hardware target such as `mi23-reva`, add a platform/hardware
selection branch in the top-level CMake metadata defaults, set:

- `MI23_PLATFORM_ID`
- `MI23_DEFAULT_HARDWARE_REVISION`
- flash and filesystem geometry
- capability defaults

Then add the matching platform implementation behind the HAL/platform boundary
and keep the companion protocol fields unchanged.

## LittleFS Boundary Protection

The Waveshare RP2350-PiZero build reserves the final 2 MiB of the 16 MiB flash
for AxiomFS/LittleFS:

```text
flash_size_bytes              = 16777216
filesystem_offset_bytes       = 14680064 / 0x00E00000
filesystem_size_bytes         = 2097152
```

RP2350 flash-layout constants statically assert this geometry, and the RP2350
CMake build runs a post-link ELF section check. If any linked flash section
would extend to or past `0x00E00000`, the build fails before UF2 output is
accepted. Firmware update flows must preserve bytes from `0x00E00000` through
the end of flash.

## Manual BOOTSEL Recovery

If normal firmware communication is unavailable:

1. Disconnect USB power from the calculator/prototype.
2. Hold the board's BOOTSEL button.
3. Connect USB while BOOTSEL is held.
4. Release BOOTSEL after the RP2350 boot volume appears on the host computer.
5. Copy the compatible `mi23.uf2` file to the mounted boot volume.
6. Wait for the device to reboot.

This recovery path uses the RP2350 ROM bootloader and does not require the
companion protocol to be working.

## Error Examples

Unknown command:

```json
{"id":15,"cmd":"bad.command"}
```

```json
{"id":15,"ok":false,"error":{"code":"unknown_command","message":"Unknown command"}}
```

Path traversal:

```json
{"id":16,"cmd":"fs.list","path":"/../"}
```

```json
{"id":16,"ok":false,"error":{"code":"path_denied","message":"Path traversal is not allowed"}}
```

Invalid setting:

```json
{"id":17,"cmd":"settings.set","values":{"exam_mode":true}}
```

```json
{"id":17,"ok":false,"error":{"code":"invalid_argument","message":"Unknown setting: exam_mode"}}
```
