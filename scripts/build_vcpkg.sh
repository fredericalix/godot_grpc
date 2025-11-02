#!/bin/bash

# Build script for godot_grpc using vcpkg (macOS/Linux)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== godot_grpc Build Script (vcpkg) ===${NC}"

# Build type
BUILD_TYPE="${1:-Release}"
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"

# Check for vcpkg
if [ -z "$VCPKG_ROOT" ]; then
    echo -e "${RED}Error: VCPKG_ROOT environment variable not set${NC}"
    echo "Please install vcpkg and set VCPKG_ROOT:"
    echo "  git clone https://github.com/microsoft/vcpkg.git"
    echo "  cd vcpkg && ./bootstrap-vcpkg.sh"
    echo "  export VCPKG_ROOT=\$(pwd)"
    exit 1
fi

echo -e "${YELLOW}vcpkg root: $VCPKG_ROOT${NC}"

# Check for godot-cpp
GODOT_CPP_DIR="$PROJECT_ROOT/godot-cpp"
if [ ! -d "$GODOT_CPP_DIR" ]; then
    echo -e "${YELLOW}godot-cpp not found, cloning...${NC}"
    git clone https://github.com/godotengine/godot-cpp "$GODOT_CPP_DIR"
    cd "$GODOT_CPP_DIR"
    git checkout 4.3
    cd "$PROJECT_ROOT"
    echo -e "${GREEN}godot-cpp cloned successfully${NC}"
fi

# Note: Dependencies will be automatically installed by CMake using vcpkg.json manifest
echo -e "${YELLOW}Dependencies will be installed automatically via vcpkg manifest${NC}"

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DUSE_VCPKG=ON \
    -DGODOT_CPP_DIR="$GODOT_CPP_DIR"

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . --config "$BUILD_TYPE" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo -e "${GREEN}=== Build completed successfully ===${NC}"
echo -e "Output: ${YELLOW}$PROJECT_ROOT/demo/addons/godot_grpc/bin/${NC}"
