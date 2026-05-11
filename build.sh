#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

show_banner() {
cat << 'EOF'

 /$$      /$$ /$$$$$$       /$$$$$$   /$$$$$$
| $$$    /$$$|_  $$_/      /$$__  $$ /$$__  $$
| $$$$  /$$$$  | $$       |__/  \ $$|__/  \ $$
| $$ $$/$$ $$  | $$ /$$$$$$ /$$$$$$/   /$$$$$/
| $$  $$$| $$  | $$|______//$$____/   |___  $$
| $$\  $ | $$  | $$       | $$       /$$  \ $$
| $$ \/  | $$ /$$$$$$     | $$$$$$$$|  $$$$$$/
|__/     |__/|______/     |________/ \______/

             MI-23 BUILD SYSTEM

EOF
}

show_help() {
    echo "Usage: ./build.sh [options]"
    echo ""
    echo "Run ./build.sh with no options for interactive mode."
    echo ""
    echo "Options:"
    echo "  --clean"
    echo "  --platform=host"
    echo "  --platform=windows"
    echo "  --platform=rp2350"
    echo "  --test"
    echo "  --run"
    echo "  --release"
    echo "  --help"
}

CLEAN=false
PLATFORM="host"
RUN_TESTS=false
RUN_AFTER=false
RELEASE=false
WINDOWS=false

interactive_menu() {
    echo "What do you want to build?"
    echo "  1) Host simulator"
    echo "  2) Host simulator and run"
    echo "  3) Run unit tests"
    echo "  4) RP2350 firmware"
    echo "  5) Windows simulator"
    echo "  6) Linux release build"
    echo ""
    read -rp "Choose an option [1-6]: " choice

    case "$choice" in
        1) PLATFORM="host" ;;
        2) PLATFORM="host"; RUN_AFTER=true ;;
        3) PLATFORM="host"; RUN_TESTS=true ;;
        4) PLATFORM="rp2350" ;;
        5) PLATFORM="windows"; WINDOWS=true ;;
        6) PLATFORM="host"; RELEASE=true ;;
        *) echo "Invalid option"; exit 1 ;;
    esac

    read -rp "Clean build folder first? [y/N]: " clean_choice
    case "$clean_choice" in
        y|Y|yes|YES) CLEAN=true ;;
    esac
}

show_banner

if [ "$#" -eq 0 ]; then
    interactive_menu
else
    for arg in "$@"; do
        case "$arg" in
            --clean) CLEAN=true ;;
            --platform=*) PLATFORM="${arg#*=}" ;;
            --test) RUN_TESTS=true; PLATFORM="host" ;;
            --run) RUN_AFTER=true ;;
            --release) RELEASE=true ;;
            --help) show_help; exit 0 ;;
            *) echo "Unknown option: $arg"; show_help; exit 1 ;;
        esac
    done
fi

if [ "$PLATFORM" = "win" ]; then
    PLATFORM="windows"
fi

if [ "$PLATFORM" = "windows" ]; then
    WINDOWS=true
fi

if [ "$RUN_TESTS" = true ] && [ "$WINDOWS" = true ]; then
    echo "Error: tests only work for native host builds."
    exit 1
fi

if [ "$RELEASE" = true ] && [ "$PLATFORM" = "rp2350" ]; then
    echo "Error: release mode is only for simulator builds."
    exit 1
fi

if [ "$WINDOWS" = true ]; then
    BUILD_DIR="$PROJECT_DIR/build-win"
elif [ "$RELEASE" = true ]; then
    BUILD_DIR="$PROJECT_DIR/build-release"
else
    BUILD_DIR="$PROJECT_DIR/build-$PLATFORM"
fi

HOST_BINARY="$BUILD_DIR/firmware/platform/host/sdl_simulator/mi23"
WINDOWS_BINARY="$BUILD_DIR/firmware/platform/host/sdl_simulator/mi23.exe"
HOST_TEST_DIR="$BUILD_DIR/tests"
RP2350_OUTPUT_DIR="$BUILD_DIR/firmware/platform/rp2350"
RP2350_UF2="$RP2350_OUTPUT_DIR/mi23.uf2"
RP2350_ELF="$RP2350_OUTPUT_DIR/mi23.elf"
RP2350_BIN="$RP2350_OUTPUT_DIR/mi23.bin"

if [ "$CLEAN" = true ]; then
    echo "Cleaning $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    echo "Configuring CMake for platform: $PLATFORM"

    if [ "$WINDOWS" = true ]; then
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM=host \
              -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/toolchains/mingw64.cmake" \
              -DCMAKE_BUILD_TYPE=Release \
              -DBUILD_TESTING=OFF

    elif [ "$RELEASE" = true ]; then
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM=host \
              -DCMAKE_BUILD_TYPE=Release \
              -DBUILD_RELEASE=ON

    else
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM="$PLATFORM"
    fi
fi

echo "Building MI-23 ($PLATFORM)..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

if [ "$WINDOWS" = true ]; then
    if [ ! -f "$WINDOWS_BINARY" ]; then
        echo "Error: Windows binary not found at $WINDOWS_BINARY"
        exit 1
    fi

    if command -v x86_64-w64-mingw32-strip &> /dev/null; then
        x86_64-w64-mingw32-strip "$WINDOWS_BINARY"
    fi

    echo ""
    echo "Windows build successful:"
    echo "  $WINDOWS_BINARY"
    exit 0
fi

if [ "$RELEASE" = true ]; then
    if [ ! -f "$HOST_BINARY" ]; then
        echo "Error: release binary not found at $HOST_BINARY"
        exit 1
    fi

    strip "$HOST_BINARY"
    cp "$HOST_BINARY" "$PROJECT_DIR/mi23-linux-x86_64"

    echo ""
    echo "Release binary ready:"
    echo "  $PROJECT_DIR/mi23-linux-x86_64"
    exit 0
fi

echo ""
echo "Build successful."

if [ "$PLATFORM" = "rp2350" ]; then
    echo ""
    echo "RP2350 firmware outputs:"
    echo "  UF2: $RP2350_UF2"
    echo "  ELF: $RP2350_ELF"
    echo "  BIN: $RP2350_BIN"
fi

if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo "Running unit tests..."
    ctest --test-dir "$HOST_TEST_DIR" --output-on-failure
fi

if [ "$RUN_AFTER" = true ]; then
    if [ "$PLATFORM" != "host" ]; then
        echo "Warning: --run only works with host builds."
    else
        "$HOST_BINARY"
    fi
fi
