#!/bin/bash

# MI-23 Build Script
# Run this from the ~/calculator/ directory

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

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

# Parse arguments
CLEAN=false
PLATFORM="host"
RUN_TESTS=false
RUN_AFTER=false
RELEASE=false
WINDOWS=false

for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN=true
            ;;
        --platform=*)
            PLATFORM="${arg#*=}"
            ;;
        --test)
            RUN_TESTS=true
            PLATFORM="host"
            ;;
        --run)
            RUN_AFTER=true
            ;;
        --release)
            RELEASE=true
            ;;
        --help)
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "  --clean            Wipe build folder before building"
            echo "  --platform=host    Build for PC simulator (default)"
            echo "  --platform=windows Cross-compile PC simulator for Windows"
            echo "  --platform=rp2350  Build for real hardware"
            echo "  --test             Build and run unit tests (forces host platform)"
            echo "  --run              Launch the simulator after building"
            echo "  --release          Build a release binary (stripped, no debug)"
            echo "  --help             Show this message"
            exit 0
            ;;
    esac
done

if [ "$PLATFORM" = "win" ]; then
    PLATFORM="windows"
fi

if [ "$PLATFORM" = "windows" ]; then
    WINDOWS=true
fi

if [ "$RUN_TESTS" = true ] && [ "$WINDOWS" = true ]; then
    echo "Error: --test is only supported for native host builds, not Windows cross-builds"
    exit 1
fi

if [ "$RELEASE" = true ] && [ "$PLATFORM" = "rp2350" ]; then
    echo "Error: --release is only supported for host and Windows simulator builds"
    exit 1
fi

# Release builds always go into their own directory so they never
# overwrite a debug build you might want to keep for development
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

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# ── Configure ────────────────────────────────────────────────────────────────
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
        # Release mode:
        #   CMAKE_BUILD_TYPE=Release   → enables -O2 optimisation, disables asserts
        #   BUILD_RELEASE=ON           → our custom flag that switches SDL2 to static
        cmake -S "$PROJECT_DIR" \
              -B "$BUILD_DIR" \
              -DPLATFORM=host \
              -DCMAKE_BUILD_TYPE=Release \
              -DBUILD_RELEASE=ON
    else
        cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DPLATFORM="$PLATFORM"
    fi
fi

# ── Build ────────────────────────────────────────────────────────────────────
echo "Building MI-23 ($PLATFORM)..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

# ── Windows post-processing ─────────────────────────────────────────────────
if [ "$WINDOWS" = true ]; then
    if [ ! -f "$WINDOWS_BINARY" ]; then
        echo "Error: Windows binary not found at $WINDOWS_BINARY"
        exit 1
    fi

    if command -v x86_64-w64-mingw32-strip &> /dev/null; then
        echo ""
        echo "Stripping Windows binary..."
        x86_64-w64-mingw32-strip "$WINDOWS_BINARY"
    fi

    if command -v x86_64-w64-mingw32-objdump &> /dev/null; then
        WINDOWS_DLL_DIR="/usr/x86_64-w64-mingw32/sys-root/mingw/bin"
        WINDOWS_OUTPUT_DIR="$(dirname "$WINDOWS_BINARY")"

        echo ""
        echo "Copying Windows runtime DLLs..."
        declare -A COPIED_DLLS=()
        DLL_SCAN_QUEUE=("$WINDOWS_BINARY")

        while [ "${#DLL_SCAN_QUEUE[@]}" -gt 0 ]; do
            current_binary="${DLL_SCAN_QUEUE[0]}"
            DLL_SCAN_QUEUE=("${DLL_SCAN_QUEUE[@]:1}")

            for dll in $(x86_64-w64-mingw32-objdump -p "$current_binary" | awk '/DLL Name:/ {print $3}'); do
                source_dll="$WINDOWS_DLL_DIR/$dll"
                output_dll="$WINDOWS_OUTPUT_DIR/$dll"

                if [ -f "$source_dll" ] && [ -z "${COPIED_DLLS[$dll]+x}" ]; then
                    cp "$source_dll" "$output_dll"
                    COPIED_DLLS[$dll]=1
                    DLL_SCAN_QUEUE+=("$output_dll")
                    echo "  $dll"
                fi
            done
        done

        # Fedora's mingw64-sdl2-compat package may load SDL3 at runtime without
        # listing it in SDL2.dll's import table, so ship it when available.
        if [ -f "$WINDOWS_OUTPUT_DIR/SDL2.dll" ] && [ -f "$WINDOWS_DLL_DIR/SDL3.dll" ]; then
            if [ ! -f "$WINDOWS_OUTPUT_DIR/SDL3.dll" ]; then
                cp "$WINDOWS_DLL_DIR/SDL3.dll" "$WINDOWS_OUTPUT_DIR/"
                echo "  SDL3.dll"
            fi
        fi
    fi

    echo ""
    echo "================================"
    echo "  Windows build successful!"
    echo "  EXE: $WINDOWS_BINARY"
    echo "================================"
    exit 0
fi

# ── Release post-processing ──────────────────────────────────────────────────
if [ "$RELEASE" = true ]; then
    BINARY="$HOST_BINARY"

    # Check that strip is available — it ships with binutils which is
    # always present on Fedora, but worth a clear error if somehow missing
    if ! command -v strip &> /dev/null; then
        echo "Error: 'strip' not found. Install binutils: sudo dnf install binutils"
        exit 1
    fi

    if [ ! -f "$BINARY" ]; then
        echo "Error: release binary not found at $BINARY"
        exit 1
    fi

    echo ""
    echo "Stripping debug symbols..."
    BEFORE=$(du -sh "$BINARY" | cut -f1)
    strip "$BINARY"
    AFTER=$(du -sh "$BINARY" | cut -f1)
    echo "  Size before strip: $BEFORE"
    echo "  Size after strip:  $AFTER"

    # Copy the finished binary to the project root so it's easy to find
    # and attach to a GitHub release
    cp "$BINARY" "$PROJECT_DIR/mi23-linux-x86_64"

    echo ""
    echo "================================"
    echo "  Release binary ready!"
    echo "  $(pwd)/mi23-linux-x86_64"
    echo "================================"
    echo ""
    echo "Users can run it with:"
    echo "  chmod +x mi23-linux-x86_64"
    echo "  ./mi23-linux-x86_64"
    exit 0
fi

echo ""
echo "================================"
echo "  Build successful!"
echo "================================"

if [ "$PLATFORM" = "rp2350" ]; then
    echo ""
    echo "RP2350 outputs:"
    echo "  UF2: $RP2350_UF2"
    echo "  ELF: $RP2350_ELF"
    echo "  BIN: $RP2350_BIN"
fi

# Run tests if --test was passed
if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo "Running unit tests..."
    echo "--------------------------------"
    ctest --test-dir "$HOST_TEST_DIR" --output-on-failure
fi

# Launch simulator if --run was passed
if [ "$RUN_AFTER" = true ]; then
    if [ "$PLATFORM" != "host" ]; then
        echo "Warning: --run only works with host platform, skipping"
    else
        echo ""
        echo "Launching simulator..."
        "$HOST_BINARY"
    fi
fi
