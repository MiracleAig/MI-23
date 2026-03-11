#!/bin/bash

# MI-23 Build Script
# Run this from the ~/calculator/ directory

set -e
# set -e means "exit immediately if any command fails"
# This prevents the script from continuing if cmake or make errors out

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# This finds the absolute path of wherever the script lives
# so the script works no matter what directory you run it from

BUILD_DIR="$PROJECT_DIR/build"

echo "================================"
echo "  MI-23 Build System"
echo "================================"

# Parse arguments
CLEAN=false
PLATFORM="host"

for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN=true
            ;;
        --platform=*)
            PLATFORM="${arg#*=}"
            ;;
        --help)
            echo "Usage: ./build.sh [options]"
            echo ""
            echo "Options:"
            echo "  --clean          Wipe build folder before building"
            echo "  --platform=host  Build for PC simulator (default)"
            echo "  --platform=rp2350  Build for real hardware"
            echo "  --help           Show this message"
            exit 0
            ;;
    esac
done

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Configure with CMake if not already configured
# or if we just cleaned
if [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Configuring with CMake for platform: $PLATFORM"
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DPLATFORM="$PLATFORM"
fi

# Build
echo "Building MI-23..."
cmake --build "$BUILD_DIR" -- -j$(nproc)
# -j$(nproc) uses all available CPU cores to build faster
# nproc is a command that returns how many CPU cores your machine has

# Check if build succeeded
if [ $? -eq 0 ]; then
    echo ""
    echo "================================"
    echo "  Build successful!"
    echo "  Binary: $BUILD_DIR/mi23"
    echo "================================"
    echo ""
    echo "Run with:"
    echo "  $BUILD_DIR/mi23"
else
    echo ""
    echo "================================"
    echo "  Build FAILED"
    echo "================================"
    exit 1
fi
