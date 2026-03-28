# Miracle's Instruments - MI-23
<div align="center">
  <img src="MI23_logo.svg" alt="MI-23 by Miracle's Instruments" width="520"/>
  <p>MI-23 is a DIY graphing calculator powered by the Raspberry Pi RP2350, built as an open-source alternative to the TI-84 Plus CE at a fraction of the cost. Inspired by NumWorks, the MI-23 targets students who need an affordable, exam-approved graphing calculator without sacrificing performance.</p>
</div>

## Features

- Arithmetic expression evaluation with correct operator precedence
- Exponent and negative number support
- Scrolling calculation history
- Blinking text cursor
- Dual-platform codebase — runs as a desktop simulator (SDL2) and on real hardware (RP2350)

## Hardware

| Part | Details |
|------|---------|
| MCU | Waveshare RP2350-PiZero (RP2350, 16MB flash) |
| Display | 2.0" ST7789 TFT 320×240 via SPI |
| Keypad | 4×4 matrix keypad (40+ key layout planned) |
| Battery | 3.7V LiPo |

---

## Running the Simulator

The simulator lets you run MI-23 on your computer without any hardware. It uses SDL2 to emulate the display and accepts keyboard input.

### Prerequisites

#### Linux (Debian/Ubuntu/Fedora)
```bash
# Debian / Ubuntu
sudo apt install cmake g++ libsdl2-dev libgtest-dev

# Fedora
sudo dnf install cmake gcc-c++ SDL2-devel gtest-devel
```

#### macOS
```bash
brew install cmake sdl2 googletest
```

#### Windows
The recommended approach on Windows is to use [MSYS2](https://www.msys2.org). After installing, open the MSYS2 MinGW 64-bit shell and run:
```bash
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-gtest
```

---

### Build and Run

```bash
git clone https://github.com/MiracleAig/MI-23
cd MI-23

cmake -B build-host -DPLATFORM=host
cmake --build build-host

./build-host/mi23
```

### Controls

| Key | Action |
|-----|--------|
| `0–9` | Enter digits |
| `+ - * /` | Operators |
| `^` | Exponent |
| `( )` | Parentheses |
| `.` | Decimal point |
| `Enter` | Evaluate expression |
| `Backspace` | Delete last character |
| `Escape` | Clear input |
| `↑ / ↓` or scroll | Scroll history |

---

## Building for Hardware

### Prerequisites

You'll need the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk) cloned somewhere on your machine.

```bash
git clone https://github.com/raspberrypi/pico-sdk ~/pico-sdk
cd ~/pico-sdk && git submodule update --init
```

### Build

```bash
cmake -B build-rp2350 -DPLATFORM=rp2350 -DPICO_SDK_PATH=/path/to/pico-sdk
cmake --build build-rp2350
```

This produces `build-rp2350/mi23.uf2`.

### Flash

1. Hold the **BOOT** button on the Waveshare RP2350-PiZero while plugging in USB
2. It mounts as a USB drive on your computer
3. Drag and drop `mi23.uf2` onto it
4. The board resets automatically and starts running

---

## Project Structure

```
firmware/
├── core/        # Expression parser and evaluator (Shunting-Yard)
├── hal/
│   ├── host/    # SDL2 simulator (display + keypad)
│   └── rp2350/  # Hardware drivers (ST7789, GPIO keypad matrix)
├── graphics/    # Font renderer
├── ui/          # Calculator app, history
└── tests/       # Unit tests for the expression engine
```

---

## Running Tests

```bash
cmake -B build-host -DPLATFORM=host
cmake --build build-host
cd build-host && ctest --output-on-failure
```

---

## License

MIT — see [LICENSE](LICENSE) for details.

*Student passion project — contributions welcome.*
