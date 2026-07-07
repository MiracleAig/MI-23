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

Device info request:

```json
{"id":1,"cmd":"device.info"}
```

Expected response fields include:

```json
{"id":1,"ok":true,"result":{"model":"MI-23","name":"MI-23","firmware":"dev","protocol":1,"transport":"usb_cdc","serial":""}}
```

Capabilities request:

```json
{"id":2,"cmd":"device.capabilities"}
```

The v1 firmware advertises implemented firmware-side handlers:

```json
{"filesystem":true,"settings":true,"terminal":true,"graphs":true,"screenshots":false,"battery":false,"firmware_update":false}
```

Ping:

```json
{"id":3,"cmd":"protocol.ping"}
```

## Storage

Storage status:

```json
{"id":4,"cmd":"storage.info"}
```

Formatting storage requires the exact confirmation string:

```json
{"id":5,"cmd":"storage.format","confirm":"FORMAT_MI23_STORAGE"}
```

The protocol never formats storage automatically.

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
