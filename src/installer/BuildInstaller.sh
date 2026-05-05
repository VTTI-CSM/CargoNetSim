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
#   --qt-path DIR       Specify Qt6 installation path (auto-detected if not set)
#   --ifw-path DIR      Specify QtIFW installation path (auto-detected if not set)
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
QT_PATH=""
IFW_PATH=""

# =============================================================================
# Auto-detection functions
# =============================================================================

# Find Qt6 installation
find_qt6() {
    local qt_search_paths=()

    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS search paths
        qt_search_paths=(
            "$HOME/Qt"/*/clang_64
            "$HOME/Qt"/*/macos
            /opt/Qt/*/clang_64
            /opt/Qt/*/macos
            /usr/local/opt/qt@6
            /opt/homebrew/opt/qt@6
        )
    else
        # Linux search paths
        qt_search_paths=(
            "$HOME/Qt"/*/gcc_64
            /opt/Qt/*/gcc_64
            /usr/lib/qt6
            /usr/lib/x86_64-linux-gnu/qt6
        )
    fi

    # Sort paths in reverse to get latest version first
    for path_pattern in "${qt_search_paths[@]}"; do
        # Expand glob and sort in reverse (latest version first)
        for qt_dir in $(ls -d $path_pattern 2>/dev/null | sort -V -r); do
            if [[ -f "$qt_dir/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
                echo "$qt_dir"
                return 0
            fi
        done
    done

    return 1
}

# Find QtIFW installation
find_qtifw() {
    local ifw_search_paths=()

    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS search paths
        ifw_search_paths=(
            "$HOME/Qt/Tools/QtInstallerFramework"/*
            /opt/Qt/Tools/QtInstallerFramework/*
            /usr/local/opt/qt-installer-framework
            /opt/homebrew/opt/qt-installer-framework
        )
    else
        # Linux search paths
        ifw_search_paths=(
            "$HOME/Qt/Tools/QtInstallerFramework"/*
            /opt/Qt/Tools/QtInstallerFramework/*
            /usr/local/QtIFW/*
        )
    fi

    # Sort paths in reverse to get latest version first
    for path_pattern in "${ifw_search_paths[@]}"; do
        for ifw_dir in $(ls -d $path_pattern 2>/dev/null | sort -V -r); do
            if [[ -x "$ifw_dir/bin/binarycreator" ]]; then
                echo "$ifw_dir"
                return 0
            fi
        done
    done

    return 1
}

# =============================================================================
# Parse command line arguments
# =============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --qt-path)
            QT_PATH="$2"
            shift 2
            ;;
        --ifw-path)
            IFW_PATH="$2"
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
            echo "  --qt-path DIR       Specify Qt6 installation path (auto-detected if not set)"
            echo "  --ifw-path DIR      Specify QtIFW installation path (auto-detected if not set)"
            echo "  --clean             Clean build directory before building"
            echo "  --jobs N            Number of parallel build jobs (default: auto-detect)"
            echo "  --help              Show this help message"
            echo ""
            echo "Environment variables:"
            echo "  QT_DIR              Alternative way to specify Qt6 path"
            echo "  QTIFWDIR            Alternative way to specify QtIFW path"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# =============================================================================
# Detect platform and settings
# =============================================================================

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

# =============================================================================
# Auto-detect Qt6 and QtIFW paths
# =============================================================================

# Qt6 path detection
if [[ -z "$QT_PATH" ]]; then
    # Check environment variable
    if [[ -n "$QT_DIR" ]]; then
        QT_PATH="$QT_DIR"
    else
        echo "Auto-detecting Qt6 installation..."
        QT_PATH=$(find_qt6) || true
    fi
fi

if [[ -z "$QT_PATH" || ! -d "$QT_PATH" ]]; then
    echo "Error: Qt6 installation not found!"
    echo ""
    echo "Please either:"
    echo "  1. Install Qt6 from https://www.qt.io/download"
    echo "  2. Specify the path with --qt-path /path/to/Qt/6.x.x/gcc_64"
    echo "  3. Set the QT_DIR environment variable"
    echo ""
    echo "Searched locations:"
    if [[ "$PLATFORM" == "macOS" ]]; then
        echo "  - \$HOME/Qt/*/clang_64"
        echo "  - /opt/Qt/*/clang_64"
    else
        echo "  - \$HOME/Qt/*/gcc_64"
        echo "  - /opt/Qt/*/gcc_64"
    fi
    exit 1
fi

# QtIFW path detection
if [[ -z "$IFW_PATH" ]]; then
    # Check environment variable
    if [[ -n "$QTIFWDIR" ]]; then
        IFW_PATH="$QTIFWDIR"
    else
        echo "Auto-detecting QtIFW installation..."
        IFW_PATH=$(find_qtifw) || true
    fi
fi

if [[ -z "$IFW_PATH" || ! -d "$IFW_PATH" ]]; then
    echo "Error: Qt Installer Framework not found!"
    echo ""
    echo "Please either:"
    echo "  1. Install QtIFW from Qt Maintenance Tool or https://download.qt.io/official_releases/qt-installer-framework/"
    echo "  2. Specify the path with --ifw-path /path/to/QtIFW/4.x"
    echo "  3. Set the QTIFWDIR environment variable"
    echo ""
    echo "Searched locations:"
    echo "  - \$HOME/Qt/Tools/QtInstallerFramework/*"
    echo "  - /opt/Qt/Tools/QtInstallerFramework/*"
    exit 1
fi

# =============================================================================
# Print configuration
# =============================================================================

echo "=============================================="
echo "CargoNetSim Installer Build Script"
echo "=============================================="
echo "Platform:        $PLATFORM"
echo "Project root:    $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Qt6 path:        $QT_PATH"
echo "QtIFW path:      $IFW_PATH"
echo "Parallel jobs:   $JOBS"
echo "=============================================="

# =============================================================================
# Build process
# =============================================================================

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
    -DCMAKE_PREFIX_PATH="$QT_PATH" \
    -DCPACK_IFW_ROOT="$IFW_PATH" \
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
