<div align="center">
<h1>Miracle's Instruments</h1>
  <img src="assets/MI-23 Logo.svg" alt="Miracle's Instruments MI-23 logo" width="200"/>
    

  <p>
    MI-23 is an open-source graphing calculator project built around the Raspberry Pi RP2350 family.
    The goal is to create an affordable, student-focused alternative to calculators like the TI-84 Plus CE,
    with a modern firmware architecture, a desktop simulator, and eventually custom calculator hardware.
  </p>
</div>

---

## Project Status

MI-23 is currently in active development.

The desktop simulator is the main development target right now. It allows the calculator UI, math engine,
keypad behavior, graphing tools, and rendering code to be tested quickly before flashing real hardware.

Current focus areas:

- Calculator app polish
- Graphing app improvements
- Settings app implementation
- Keypad/input behavior
- RP2350 hardware support
- Custom PCB planning

---

## Features

Current and in-progress features include:

- Arithmetic expression evaluation with operator precedence
- Parentheses and decimal number support
- Calculator history
- Fraction input mode
- Custom keypad UI
- Graphing app with function input
- SDL-based desktop simulator
- RP2350 firmware target
- Custom ST7789 display driver
- Modular HAL architecture
- Google Test-based unit tests
- Linux and Windows simulator builds

Planned features:

- Persistent settings
- Exam mode
- File/app storage system
- Custom calculator PCB
- Final 40+ key calculator keypad

---

## Hardware

### Current Prototype Hardware

| Part | Details |
|------|---------|
| MCU | Waveshare RP2350-PiZero |
| Chip | Raspberry Pi RP2350 |
| Display | 2.0" ST7789 TFT, 320×240, SPI |
| Keypad | Prototype matrix keypad |
| Battery | 3.7V LiPo |
| Firmware format | UF2 |

The current prototype uses a Waveshare RP2350-PiZero board for bring-up. This makes it easier to test the display,
keypad, firmware, and simulator/hardware abstraction before moving to a fully custom PCB.

### Planned Custom Hardware

The long-term goal is a custom calculator PCB using an RP2350-family microcontroller.

Planned or considered hardware areas:

- RP2350B or RP2354B MCU
- External flash or storage IC for apps, files, and assets
- USB-C connector
- Battery charging and power-path management
- ST7789 or similar color display
- Full calculator keypad matrix
- Debug/programming access
- 2-layer PCB layout
- Custom enclosure

The RP2354 variants include internal flash, while RP2350 variants require external flash. The design may still include
external storage for user files, apps, fonts, icons, and other calculator assets.

---

## Software Architecture

MI-23 is designed around a HAL, or Hardware Abstraction Layer.

The HAL separates hardware-specific code from calculator logic. This means the same calculator apps, math engine,
and UI code can run in both:

- the desktop simulator
- the RP2350 hardware firmware

Platform-specific code lives under the platform layer, while shared app logic stays independent from SDL2, GPIO, SPI,
and other hardware details.

Major software areas:

| Area | Purpose |
|------|---------|
| Calculator app | Standard calculator interface |
| Graphing app | Function graphing and graph UI |
| Settings app | User configuration and device options |
| Math engine | Expression parsing and evaluation |
| Graphics | Drawing primitives, text rendering, and UI rendering |
| HAL | Shared display/input interfaces |
| Drivers | Low-level hardware drivers such as ST7789 |
| Platform | SDL simulator and RP2350 implementations |

---
# Getting Started
## Useful Resources

### Development Tools

* [Git](https://git-scm.com/)
* [CMake](https://cmake.org/)
* [SDL2](https://www.libsdl.org/)
* [Google Test](https://github.com/google/googletest)

### RP2350 Development

* [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
* [RP2350 Documentation](https://www.raspberrypi.com/documentation/microcontrollers/)
* [Pico Examples](https://github.com/raspberrypi/pico-examples)

### Recommended Development Environments

* [CLion](https://www.jetbrains.com/clion/) **recommended**
* [Visual Studio Code](https://code.visualstudio.com/)
* [Visual Studio](https://visualstudio.microsoft.com/)
* [Qt Creator](https://www.qt.io/product/development-tools)

### Platform Setup

#### Fedora Linux

```bash
sudo dnf install git cmake gcc-c++ SDL2-devel gtest-devel make
```

#### Ubuntu / Debian Linux

```bash
sudo apt update
sudo apt install git cmake g++ libsdl2-dev libgtest-dev make
```

#### Arch Linux

```bash
sudo pacman -S --needed git cmake gcc sdl2 gtest make
```

#### macOS

```bash
brew install git cmake sdl2 googletest
```

#### Windows (MSYS2)

1. Install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 MinGW 64-bit** shell
3. Run:

```bash
pacman -Syu
```

4. Then install the required packages:

```bash
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-SDL2 \
  mingw-w64-x86_64-gtest \
  mingw-w64-x86_64-make \
  git
```


## Repository Structure

```text
firmware/
├── app/
│   ├── calculator/          # Standard calculator mode
│   ├── graphing/            # Graphing app
│   └── settings/            # Settings app
├── drivers/
│   └── st7789/              # Custom ST7789 display driver
├── graphics/                # Rendering utilities and primitives
├── hal/                     # Hardware abstraction interfaces
├── math/                    # Expression parser and evaluation engine
└── platform/
    ├── host/
    │   └── sdl_simulator/   # Desktop simulator backend
    └── rp2350/
        └── config/          # RP2350 build/config definitions