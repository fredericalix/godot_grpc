#!/bin/bash

# Build script for godot_grpc using Conan (macOS/Linux)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== godot_grpc Build Script (Conan) ===${NC}"

# Build type
BUILD_TYPE="${1:-Release}"
echo -e "${YELLOW}Build type: $BUILD_TYPE${NC}"

# Check for Conan
if ! command -v conan &> /dev/null; then
    echo -e "${RED}Error: Conan not found${NC}"
    echo "Please install Conan:"
    echo "  pip install conan"
    exit 1
fi

echo -e "${YELLOW}Conan version: $(conan --version)${NC}"

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

# Create conanfile.txt if it doesn't exist
CONANFILE="$PROJECT_ROOT/conanfile.txt"
if [ ! -f "$CONANFILE" ]; then
    echo -e "${YELLOW}Creating conanfile.txt...${NC}"
    cat > "$CONANFILE" <<EOF
[requires]
grpc/1.54.3
protobuf/3.21.12
abseil/20230125.3

[generators]
cmake
cmake_find_package

[options]
grpc:shared=False
protobuf:shared=False
EOF
fi

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Install dependencies
echo -e "${YELLOW}Installing dependencies via Conan...${NC}"
conan install .. --build=missing -s build_type="$BUILD_TYPE"

# Configure
echo -e "${YELLOW}Configuring CMake...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DUSE_CONAN=ON \
    -DUSE_VCPKG=OFF \
    -DGODOT_CPP_DIR="$GODOT_CPP_DIR"

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . --config "$BUILD_TYPE" -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

echo -e "${GREEN}=== Build completed successfully ===${NC}"
echo -e "Output: ${YELLOW}$PROJECT_ROOT/demo/addons/godot_grpc/bin/${NC}"
