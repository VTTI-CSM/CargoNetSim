#!/bin/bash
# =============================================================================
# CargoNetSim Installer Build Script (macOS/Linux)
# =============================================================================
# This script builds the CargoNetSim application and generates a platform-
# specific installer using Qt Installer Framework (QtIFW).
#
# Requirements:
#   - CMake 3.24 or later
#   - Qt6 (6.3+ recommended for qt_generate_deploy_app_script)
#   - Qt Installer Framework 4.x
#   - C++17 capable compiler
#
# Usage:
#   ./BuildInstaller.sh [options]
#
# Options:
#   --build-dir DIR     Specify build directory (default: ../../build)
#   --clean             Clean build directory before building
#   --jobs N            Number of parallel build jobs (default: auto-detect)
#   --help              Show this help message
# =============================================================================

set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."

# Default values
BUILD_DIR="${PROJECT_ROOT}/build"
CLEAN_BUILD=false
JOBS=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --build-dir DIR     Specify build directory (default: ../../build)"
            echo "  --clean             Clean build directory before building"
            echo "  --jobs N            Number of parallel build jobs (default: auto-detect)"
            echo "  --help              Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Detect number of CPU cores for parallel builds
if [[ -z "$JOBS" ]]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
    else
        JOBS=$(nproc 2>/dev/null || echo 4)
    fi
fi

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    INSTALLER_EXT=".dmg"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    INSTALLER_EXT=".run"
else
    echo "Error: Unsupported platform: $OSTYPE"
    echo "This script supports macOS and Linux only."
    echo "For Windows, use BuildInstaller.bat"
    exit 1
fi

echo "=============================================="
echo "CargoNetSim Installer Build Script"
echo "=============================================="
echo "Platform:        $PLATFORM"
echo "Project root:    $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Parallel jobs:   $JOBS"
echo "=============================================="

# Clean build directory if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    echo ""
    echo "=== Cleaning build directory ==="
    rm -rf "$BUILD_DIR"
fi

# Create build directory
echo ""
echo "=== Creating build directory ==="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo ""
echo "=== Configuring CMake ==="
cmake "$PROJECT_ROOT" \
    -DCARGONET_BUILD_INSTALLER=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${BUILD_DIR}/install"

# Check if configuration was successful
if [[ $? -ne 0 ]]; then
    echo "Error: CMake configuration failed"
    exit 1
fi

# Build
echo ""
echo "=== Building CargoNetSim ==="
cmake --build . --config Release -j"$JOBS"

if [[ $? -ne 0 ]]; then
    echo "Error: Build failed"
    exit 1
fi

# Install to staging directory
echo ""
echo "=== Installing to staging directory ==="
cmake --build . --target install --config Release

if [[ $? -ne 0 ]]; then
    echo "Error: Installation to staging directory failed"
    exit 1
fi

# Generate installer using CPack with IFW generator
echo ""
echo "=== Generating installer ==="
cpack -G IFW -C Release

if [[ $? -ne 0 ]]; then
    echo "Error: CPack installer generation failed"
    echo ""
    echo "Make sure Qt Installer Framework (QtIFW) is installed."
    echo "You can set CPACK_IFW_ROOT to specify the QtIFW installation path."
    echo ""
    echo "Installation paths checked:"
    if [[ "$PLATFORM" == "macOS" ]]; then
        echo "  - \$HOME/Qt/Tools/QtInstallerFramework/4.x"
        echo "  - /opt/Qt/Tools/QtInstallerFramework/4.x"
        echo "  - /usr/local/opt/qt-installer-framework"
    else
        echo "  - \$HOME/Qt/Tools/QtInstallerFramework/4.x"
        echo "  - /opt/Qt/Tools/QtInstallerFramework/4.x"
    fi
    exit 1
fi

# Find the generated installer
echo ""
echo "=== Build Complete ==="
INSTALLER_FILE=$(find "$BUILD_DIR" -maxdepth 1 -name "CargoNetSim*${INSTALLER_EXT}" -type f 2>/dev/null | head -1)

if [[ -n "$INSTALLER_FILE" ]]; then
    echo "Installer generated successfully:"
    echo "  $INSTALLER_FILE"
    echo ""
    echo "File size: $(du -h "$INSTALLER_FILE" | cut -f1)"

    if [[ "$PLATFORM" == "macOS" ]]; then
        echo ""
        echo "To install, double-click the .dmg file and drag CargoNetSim to Applications."
    else
        echo ""
        echo "To install, run:"
        echo "  chmod +x \"$INSTALLER_FILE\""
        echo "  \"$INSTALLER_FILE\""
    fi
else
    echo "Warning: Could not locate the generated installer file."
    echo "Check the build directory for installer output:"
    ls -la "$BUILD_DIR"/*.dmg "$BUILD_DIR"/*.run 2>/dev/null || echo "No installer files found."
fi

echo ""
echo "=============================================="
echo "Build completed successfully!"
echo "=============================================="
