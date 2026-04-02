#!/bin/bash

# MI-23 Build Script
# Run this from the ~/calculator/ directory

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "================================"
echo "  MI-23 Build System"
echo "================================"

# Parse arguments
CLEAN=false
PLATFORM="host"
RUN_TESTS=false
RUN_AFTER=false
RELEASE=false

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
            PLATFORM="host"
            ;;
        --help)
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "  --clean            Wipe build folder before building"
            echo "  --platform=host    Build for PC simulator (default)"
            echo "  --platform=rp2350  Build for real hardware"
            echo "  --test             Build and run unit tests (forces host platform)"
            echo "  --run              Launch the simulator after building"
            echo "  --release          Build a release binary (static, stripped, no debug)"
            echo "  --help             Show this message"
            exit 0
            ;;
    esac
done

# Release builds always go into their own directory so they never
# overwrite a debug build you might want to keep for development
if [ "$RELEASE" = true ]; then
    BUILD_DIR="$PROJECT_DIR/build-release"
else
    BUILD_DIR="$PROJECT_DIR/build-$PLATFORM"
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# ── Configure ────────────────────────────────────────────────────────────────
if [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Configuring CMake for platform: $PLATFORM"

    if [ "$RELEASE" = true ]; then
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
cmake --build "$BUILD_DIR" -- -j$(nproc)

# ── Release post-processing ──────────────────────────────────────────────────
if [ "$RELEASE" = true ]; then
    BINARY="$BUILD_DIR/mi23"

    # Check that strip is available — it ships with binutils which is
    # always present on Fedora, but worth a clear error if somehow missing
    if ! command -v strip &> /dev/null; then
        echo "Error: 'strip' not found. Install binutils: sudo dnf install binutils"
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

# Run tests if --test was passed
if [ "$RUN_TESTS" = true ]; then
    echo ""
    echo "Running unit tests..."
    echo "--------------------------------"
    cd "$BUILD_DIR"
    ctest --output-on-failure
    cd "$PROJECT_DIR"
fi

# Launch simulator if --run was passed
if [ "$RUN_AFTER" = true ]; then
    if [ "$PLATFORM" != "host" ]; then
        echo "Warning: --run only works with host platform, skipping"
    else
        echo ""
        echo "Launching simulator..."
        "$BUILD_DIR/mi23"
    fi