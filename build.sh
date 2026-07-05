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
    echo "  --developer"
    echo "  --dev"
    echo "  --help"
}

CLEAN=false
PLATFORM="host"
RUN_TESTS=false
RUN_AFTER=false
RELEASE=false
DEVELOPER=false
WINDOWS=false

interactive_menu() {
    echo "What do you want to build?"
    echo "  1) Host simulator developer build"
    echo "  2) Host simulator developer build and run"
    echo "  3) Run unit tests"
    echo "  4) RP2350 developer firmware"
    echo "  5) Host simulator release build"
    echo "  6) RP2350 release firmware"
    echo "  7) Windows simulator"
    echo ""
    read -rp "Choose an option [1-7]: " choice

    case "$choice" in
        1) PLATFORM="host"; DEVELOPER=true ;;
        2) PLATFORM="host"; DEVELOPER=true; RUN_AFTER=true ;;
        3) PLATFORM="host"; RUN_TESTS=true ;;
        4) PLATFORM="rp2350"; DEVELOPER=true ;;
        5) PLATFORM="host"; RELEASE=true ;;
        6) PLATFORM="rp2350"; RELEASE=true ;;
        7) PLATFORM="windows"; WINDOWS=true ;;
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
            --developer|--dev) DEVELOPER=true ;;
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

if [ "$RELEASE" = true ] && [ "$DEVELOPER" = true ]; then
    echo "Error: --release and --developer cannot be used together."
    exit 1
fi

if [ "$RUN_TESTS" = true ] && [ "$WINDOWS" = true ]; then
    echo "Error: tests only work for native host builds."
    exit 1
fi

if [ "$RUN_TESTS" = true ] && [ "$RELEASE" = true ]; then
    echo "Error: --test uses a host test build, not a release build."
    exit 1
fi

if [ "$WINDOWS" = true ]; then
    BUILD_DIR="$PROJECT_DIR/build-win"
elif [ "$RELEASE" = true ] && [ "$PLATFORM" = "host" ]; then
    BUILD_DIR="$PROJECT_DIR/build-host-release"
elif [ "$RELEASE" = true ] && [ "$PLATFORM" = "rp2350" ]; then
    BUILD_DIR="$PROJECT_DIR/build-rp2350-release"
elif [ "$DEVELOPER" = true ]; then
    BUILD_DIR="$PROJECT_DIR/build-$PLATFORM-dev"
else
    BUILD_DIR="$PROJECT_DIR/build-$PLATFORM"
fi

DEVELOPER_OPTIONS="OFF"
BUILD_TYPE="Debug"
BUILD_LABEL="standard"
if [ "$DEVELOPER" = true ]; then
    DEVELOPER_OPTIONS="ON"
    BUILD_LABEL="developer"
elif [ "$RELEASE" = true ]; then
    BUILD_TYPE="Release"
    BUILD_LABEL="release"
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

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ] || { [ ! -f "$BUILD_DIR/Makefile" ] && [ ! -f "$BUILD_DIR/build.ninja" ]; }; then
    echo "Configuring CMake for platform: $PLATFORM"

    if [ "$WINDOWS" = true ]; then
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM=host \
              -DCMAKE_TOOLCHAIN_FILE="$PROJECT_DIR/cmake/toolchains/mingw64.cmake" \
              -DCMAKE_BUILD_TYPE=Release \
              -DBUILD_TESTING=OFF \
              -DMI23_ENABLE_DEVELOPER_OPTIONS=OFF

    elif [ "$PLATFORM" = "host" ]; then
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM=host \
              -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
              -DBUILD_RELEASE="$RELEASE" \
              -DMI23_ENABLE_DEVELOPER_OPTIONS="$DEVELOPER_OPTIONS"

    else
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM="$PLATFORM" \
              -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
              -DMI23_ENABLE_DEVELOPER_OPTIONS="$DEVELOPER_OPTIONS"
    fi
fi

echo "Building MI-23 ($PLATFORM, $BUILD_LABEL)..."
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
    if [ "$PLATFORM" = "host" ]; then
        if [ ! -f "$HOST_BINARY" ]; then
            echo "Error: release binary not found at $HOST_BINARY"
            exit 1
        fi

        strip "$HOST_BINARY"
        cp "$HOST_BINARY" "$PROJECT_DIR/mi23-linux-x86_64"

        echo ""
        echo "Host release binary ready:"
        echo "  $PROJECT_DIR/mi23-linux-x86_64"
        echo "Developer Options: disabled"
    elif [ "$PLATFORM" = "rp2350" ]; then
        if [ ! -f "$RP2350_UF2" ]; then
            echo "Error: RP2350 release firmware not found at $RP2350_UF2"
            exit 1
        fi

        echo ""
        echo "RP2350 release firmware ready:"
        echo "  UF2: $RP2350_UF2"
        echo "  ELF: $RP2350_ELF"
        echo "  BIN: $RP2350_BIN"
        echo "Developer Options: disabled"
    fi
    exit 0
fi

echo ""
echo "Build successful."

if [ "$DEVELOPER" = true ]; then
    echo "Developer Options: enabled"
else
    echo "Developer Options: disabled"
fi

if [ "$PLATFORM" = "rp2350" ]; then
    echo ""
    echo "RP2350 $BUILD_LABEL firmware outputs:"
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
