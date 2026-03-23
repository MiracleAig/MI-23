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
            # Tests can only run on host, so force the platform
            ;;
        --run)
            RUN_AFTER=true
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
            echo "  --help             Show this message"
            exit 0
            ;;
    esac
done

# Each platform gets its own build directory so they never overwrite each other
# build-host/ for the simulator, build-rp2350/ for real hardware
BUILD_DIR="$PROJECT_DIR/build-$PLATFORM"

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning $BUILD_DIR..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

# Configure with CMake if not already configured or if we just cleaned
if [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Configuring CMake for platform: $PLATFORM"
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DPLATFORM="$PLATFORM"
fi

# Build
echo "Building MI-23 ($PLATFORM)..."
cmake --build "$BUILD_DIR" -- -j$(nproc)

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
fi