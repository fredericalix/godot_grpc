#!/bin/bash

# Build script for macOS Universal Binary (arm64 + x86_64)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== godot_grpc Universal Binary Build Script ===${NC}"
echo -e "${BLUE}This will build for both arm64 and x86_64 architectures${NC}"

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

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo -e "${RED}Error: This script only works on macOS${NC}"
    exit 1
fi

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

# Function to install vcpkg dependencies for an architecture
install_vcpkg_deps() {
    local TRIPLET=$1
    echo -e "${YELLOW}Installing vcpkg dependencies for $TRIPLET...${NC}"

    cd "$VCPKG_ROOT"

    # Check if already installed
    if [ -d "installed/$TRIPLET" ]; then
        echo -e "${BLUE}Dependencies for $TRIPLET already installed${NC}"
        return
    fi

    echo -e "${YELLOW}This may take 30-60 minutes...${NC}"
    ./vcpkg install grpc:$TRIPLET protobuf:$TRIPLET abseil:$TRIPLET

    cd "$PROJECT_ROOT"
}

# Function to build godot-cpp for an architecture
build_godot_cpp() {
    local ARCH=$1
    local TARGET=$2

    echo -e "${YELLOW}Building godot-cpp for $ARCH ($TARGET)...${NC}"

    cd "$GODOT_CPP_DIR"

    # Check if already built
    if [ -f "bin/libgodot-cpp.macos.$TARGET.$ARCH.a" ]; then
        echo -e "${BLUE}godot-cpp for $ARCH ($TARGET) already built${NC}"
        cd "$PROJECT_ROOT"
        return
    fi

    scons platform=macos arch=$ARCH target=$TARGET -j$(sysctl -n hw.ncpu)

    cd "$PROJECT_ROOT"
}

# Function to build godot_grpc for an architecture
build_godot_grpc() {
    local ARCH=$1
    local TRIPLET=$2

    echo -e "${YELLOW}Building godot_grpc for $ARCH...${NC}"

    BUILD_DIR="$PROJECT_ROOT/build_$ARCH"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Configure
    cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
        -DVCPKG_TARGET_TRIPLET="$TRIPLET" \
        -DUSE_VCPKG=ON \
        -DGODOT_CPP_DIR="$GODOT_CPP_DIR"

    # Build
    cmake --build . --config "$BUILD_TYPE" -j$(sysctl -n hw.ncpu)

    cd "$PROJECT_ROOT"
}

# Main build process
echo ""
echo -e "${GREEN}Step 1/5: Installing vcpkg dependencies${NC}"
echo -e "${BLUE}Installing for arm64...${NC}"
install_vcpkg_deps "arm64-osx"

echo ""
echo -e "${BLUE}Installing for x86_64...${NC}"
install_vcpkg_deps "x64-osx"

echo ""
echo -e "${GREEN}Step 2/5: Building godot-cpp${NC}"
TARGET_TYPE="template_release"
if [ "$BUILD_TYPE" == "Debug" ]; then
    TARGET_TYPE="template_debug"
fi

echo -e "${BLUE}Building godot-cpp for arm64...${NC}"
build_godot_cpp "arm64" "$TARGET_TYPE"

echo -e "${BLUE}Building godot-cpp for x86_64...${NC}"
build_godot_cpp "x86_64" "$TARGET_TYPE"

echo ""
echo -e "${GREEN}Step 3/5: Building godot_grpc${NC}"
echo -e "${BLUE}Building for arm64...${NC}"
build_godot_grpc "arm64" "arm64-osx"

echo -e "${BLUE}Building for x86_64...${NC}"
build_godot_grpc "x86_64" "x64-osx"

echo ""
echo -e "${GREEN}Step 4/5: Creating universal binary${NC}"

# Determine library name based on build type
if [ "$BUILD_TYPE" == "Debug" ]; then
    LIB_SUFFIX="template_debug"
else
    LIB_SUFFIX="template_release"
fi

ARM64_LIB="$PROJECT_ROOT/build_arm64/demo/addons/godot_grpc/bin/macos/libgodot_grpc.macos.$LIB_SUFFIX.arm64.dylib"
X64_LIB="$PROJECT_ROOT/build_x86_64/demo/addons/godot_grpc/bin/macos/libgodot_grpc.macos.$LIB_SUFFIX.x86_64.dylib"
UNIVERSAL_LIB="$PROJECT_ROOT/demo/addons/godot_grpc/bin/macos/libgodot_grpc.macos.$LIB_SUFFIX.universal.dylib"

# Check if both binaries exist
if [ ! -f "$ARM64_LIB" ]; then
    echo -e "${RED}Error: arm64 binary not found at $ARM64_LIB${NC}"
    exit 1
fi

if [ ! -f "$X64_LIB" ]; then
    echo -e "${RED}Error: x86_64 binary not found at $X64_LIB${NC}"
    exit 1
fi

# Create universal binary
echo -e "${YELLOW}Combining binaries with lipo...${NC}"
mkdir -p "$(dirname "$UNIVERSAL_LIB")"
lipo -create "$ARM64_LIB" "$X64_LIB" -output "$UNIVERSAL_LIB"

echo ""
echo -e "${GREEN}Step 5/5: Verifying universal binary${NC}"

# Verify the universal binary
echo -e "${YELLOW}Architectures in universal binary:${NC}"
lipo -archs "$UNIVERSAL_LIB"

echo -e "${YELLOW}Detailed info:${NC}"
lipo -info "$UNIVERSAL_LIB"

echo -e "${YELLOW}File size:${NC}"
ls -lh "$UNIVERSAL_LIB" | awk '{print $5}'

echo ""
echo -e "${GREEN}=== Build completed successfully ===${NC}"
echo -e "Universal binary: ${YELLOW}$UNIVERSAL_LIB${NC}"
echo ""
echo -e "${BLUE}Note: The universal binary is approximately 2x the size of a single-architecture binary.${NC}"
echo -e "${BLUE}For releases, you may want to ship both separately to reduce download size.${NC}"
echo ""
echo -e "${GREEN}To use the universal binary in Godot:${NC}"
echo -e "  1. Update godot_grpc.gdextension to reference the universal binary"
echo -e "  2. Or keep separate binaries and let Godot pick the correct one automatically"
