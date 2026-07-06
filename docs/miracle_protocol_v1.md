# Miracle Protocol v1

This document describes the first MI-23 firmware support for Miracle's Companion over USB CDC serial.

## Transport

- Physical/logical transport: USB CDC serial.
- Encoding: ASCII text.
- Framing: one command per line, terminated by `\n` or `\r\n`.
- Responses are line-oriented and terminate multi-line responses with `END`.
- Commands are uppercase tokens with no arguments in this alpha version.
- Firmware protects its input buffer with a 96-byte maximum command line and discards overlong lines.
- The CDC stream is reserved for protocol traffic while Companion Link mode is active. Firmware diagnostics should be disabled, redirected, or explicitly gated so they do not appear before, inside, or after protocol responses.

## Discovery Commands

### PING

Request:

```text
PING
```

Response:

```text
OK PONG
```

### HELLO

Request:

```text
HELLO
```

Response:

```text
OK MIRACLE_PROTOCOL 1
DEVICE_TYPE calculator
MODEL MI-23
FIRMWARE <firmware version>
HARDWARE <hardware revision>
CAPABILITIES filesystem,graphs,settings,terminal
END
```

### INFO

Request:

```text
INFO
```

Response:

```text
OK INFO
STORAGE_TOTAL <bytes>
STORAGE_USED <bytes>
STORAGE_FREE <bytes>
FILESYSTEM littlefs
END
```

Storage values come from the MI-23 filesystem abstraction. File transfer commands are intentionally not part of this first protocol slice.

## Errors

The alpha firmware may return:

```text
ERR MALFORMED
ERR UNKNOWN_COMMAND
ERR LINE_TOO_LONG
```

Companion clients may ignore leading diagnostic output from older firmware builds, but MI-23 firmware should keep current Companion Link responses free of diagnostic lines.
