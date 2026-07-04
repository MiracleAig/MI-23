# AGENTS.md

## Project Summary

MI-23 is a C++17 graphing calculator project with two primary targets:

- `host`: SDL-based desktop simulator used for day-to-day development
- `rp2350`: Raspberry Pi RP2350 firmware target

The repository is built with CMake. Shared calculator logic lives under `firmware/`, and host-only tests live under `tests/`.

## Repository Layout

- `firmware/app/`: calculator, graphing, home, and settings application logic
- `firmware/math/`: expression parsing, evaluation, and math typesetting
- `firmware/hal/`: shared display/input/settings abstractions
- `firmware/platform/host/`: SDL simulator implementation
- `firmware/platform/rp2350/`: RP2350 platform implementation
- `firmware/drivers/`: hardware display drivers
- `tests/`: Google Test coverage for host builds
- `cmake/`: host, RP2350, and toolchain CMake files
- `build.sh`: main build entrypoint

## Build and Test

Prefer the host target unless the task is explicitly about embedded hardware.

Common commands:

```bash
./build.sh --platform=host
./build.sh --platform=host --run
./build.sh --test
./build.sh --platform=rp2350
```

Direct CMake usage is also valid:

```bash
cmake -S . -B build-host -DPLATFORM=host
cmake --build build-host --parallel "$(nproc)"
ctest --test-dir build-host/tests --output-on-failure
```

RP2350 builds require `PICO_SDK_PATH` to be set to a valid Pico SDK checkout.

## Working Rules

- Keep changes narrowly scoped to the area under request.
- Do not revert unrelated user changes already present in the worktree.
- Follow existing local patterns before introducing new abstractions.
- Prefer adding or updating host-side tests when changing shared logic.
- Validate behavior with the smallest relevant build or test command before finishing.

## Targeting Guidance

- Use `host` for UI logic, math engine work, app behavior, and most regression checks.
- Use `rp2350` only when touching hardware startup, keypad, display, pin config, or low-level drivers.
- If a change affects shared code used by both targets, verify on host first.

## Notes for Agents

- The repo may be dirty. Inspect `git status` before editing and work around unrelated changes.
- Current development emphasis is the desktop simulator, not hardware-first bring-up.
- Tests are only configured for native host builds.
