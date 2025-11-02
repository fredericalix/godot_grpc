# Cross-Compilation Guide

This guide explains how to build godot_grpc for multiple platforms.

## What You Can Build from macOS Silicon

### ✅ macOS Universal Binary (arm64 + x86_64)

You can build a universal binary that works on both Apple Silicon and Intel Macs:

```bash
# 1. Install dependencies for both architectures in vcpkg
export VCPKG_ROOT=/path/to/vcpkg

# Install for arm64 (your native arch)
$VCPKG_ROOT/vcpkg install grpc:arm64-osx protobuf:arm64-osx abseil:arm64-osx

# Install for x86_64
$VCPKG_ROOT/vcpkg install grpc:x64-osx protobuf:x64-osx abseil:x64-osx

# 2. Build godot-cpp for both architectures
cd godot-cpp

# Build for arm64
scons platform=macos arch=arm64 target=template_release -j$(sysctl -n hw.ncpu)
scons platform=macos arch=arm64 target=template_debug -j$(sysctl -n hw.ncpu)

# Build for x86_64
scons platform=macos arch=x86_64 target=template_release -j$(sysctl -n hw.ncpu)
scons platform=macos arch=x86_64 target=template_debug -j$(sysctl -n hw.ncpu)

cd ..

# 3. Build godot_grpc as universal binary
mkdir build_universal && cd build_universal

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DUSE_VCPKG=ON

cmake --build . --config Release -j$(sysctl -n hw.ncpu)
```

**Note**: This creates a ~34MB universal binary (2x the size of single-arch).

### ✅ iOS Build (Experimental)

iOS builds require Xcode and proper code signing. This is **experimental** and not fully tested:

```bash
# 1. Install iOS toolchain for vcpkg
export VCPKG_ROOT=/path/to/vcpkg

# Install dependencies for iOS
$VCPKG_ROOT/vcpkg install grpc:arm64-ios protobuf:arm64-ios abseil:arm64-ios

# 2. Build godot-cpp for iOS
cd godot-cpp
scons platform=ios arch=arm64 target=template_release -j$(sysctl -n hw.ncpu)
cd ..

# 3. Configure CMake for iOS
mkdir build_ios && cd build_ios

cmake .. \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DUSE_VCPKG=ON

cmake --build . --config Release
```

**Limitations**: iOS builds require additional configuration for code signing and may not work without modifications to CMakeLists.txt.

## What Requires Native Compilation

### ❌ Linux

**Why**: Different system libraries (glibc vs macOS libc), kernel ABI, dynamic linker paths.

**Solution**: Use GitHub Actions (Ubuntu runner) or a Linux VM/container.

**Docker approach** (if you want to try locally):
```bash
docker run -it -v $(pwd):/workspace ubuntu:22.04 bash

# Inside container:
apt-get update
apt-get install -y build-essential cmake git pkg-config libssl-dev zlib1g-dev curl zip unzip tar

cd /workspace
export VCPKG_ROOT=/workspace/vcpkg
git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT
cd $VCPKG_ROOT && ./bootstrap-vcpkg.sh

cd /workspace
./scripts/build_vcpkg.sh Release
```

This works but is slow and may have issues with vcpkg binary caching.

### ❌ Windows

**Why**: Completely different toolchain (MSVC vs Clang), different binary format (PE vs Mach-O).

**Solution**: Use GitHub Actions (Windows runner) or a Windows VM.

**Not recommended**: Using mingw-w64 on macOS is theoretically possible but very unreliable for C++ projects with heavy dependencies like gRPC.

### ⚠️ Android (Possible but Complex)

Android builds require the Android NDK and special vcpkg triplets. This is **very complex** and not recommended for manual builds.

**If you really need it**:
1. Install Android NDK
2. Create custom vcpkg triplet for Android
3. Build godot-cpp for Android
4. Modify CMakeLists.txt to use NDK toolchain

**Recommendation**: Wait for community contributions or use GitHub Actions with Android SDK/NDK setup.

## Recommended Workflow for Releases

### Option 1: GitHub Actions (Automated)

The included `.github/workflows/build-release.yml` handles all platforms automatically:

1. Push a tag: `git tag v0.1.0 && git push origin v0.1.0`
2. GitHub Actions builds for macOS (arm64 + x86_64), Linux, Windows
3. Automatically creates a release with all binaries

**Platforms supported**:
- ✅ macOS arm64
- ✅ macOS x86_64
- ✅ Linux x86_64
- ✅ Windows x64

### Option 2: Manual Builds

Build what you can locally, use VMs/cloud for others:

1. **macOS** (local): Build universal binary as shown above
2. **Linux**: Rent an Ubuntu cloud VM (AWS, DigitalOcean, etc.) or use Docker
3. **Windows**: Use Parallels/VMware with Windows 11 or rent a Windows cloud VM
4. **iOS**: Local (if needed)
5. **Android**: Use GitHub Actions or skip for now

### Option 3: Hybrid Approach

- Build macOS locally
- Use GitHub Actions for Linux/Windows
- Manually download artifacts and create release

## Platform Priority for Initial Release

Consider releasing in phases:

**Phase 1 (MVP)**:
- ✅ macOS arm64 (Apple Silicon) - your primary platform
- ✅ macOS x86_64 (Intel) - easy to add via universal binary
- ✅ Linux x86_64 - via GitHub Actions
- ✅ Windows x64 - via GitHub Actions

**Phase 2 (Mobile)**:
- iOS (requires testing and configuration)
- Android (requires NDK setup and testing)

**Reasoning**: Desktop platforms (macOS, Linux, Windows) are the primary targets for Godot development. Mobile platforms require additional configuration and testing.

## Testing Before Release

Before publishing a release, verify each binary:

### macOS
```bash
file demo/addons/godot_grpc/bin/macos/*.dylib
# Should show: Mach-O 64-bit dynamically linked shared library arm64
# Or for universal: Mach-O universal binary with 2 architectures

lipo -archs demo/addons/godot_grpc/bin/macos/*.dylib
# Should show: arm64 x86_64 (for universal)
```

### Linux
```bash
file demo/addons/godot_grpc/bin/linux/*.so
# Should show: ELF 64-bit LSB shared object, x86-64

ldd demo/addons/godot_grpc/bin/linux/*.so
# Check for unresolved dependencies
```

### Windows
```bash
file demo/addons/godot_grpc/bin/windows/*.dll
# Should show: PE32+ executable (DLL) (console) x86-64
```

## Troubleshooting

### vcpkg takes forever to build dependencies

**Solution**: Use binary caching:
```bash
export VCPKG_BINARY_SOURCES="clear;files,$HOME/.vcpkg-cache,readwrite"
mkdir -p $HOME/.vcpkg-cache
```

### CMake can't find dependencies for cross-compilation

**Solution**: Make sure you installed the correct vcpkg triplet:
- macOS arm64: `arm64-osx`
- macOS x86_64: `x64-osx`
- Linux: `x64-linux`
- Windows: `x64-windows`

### Universal binary build fails

**Solution**: Build each architecture separately first, then combine:
```bash
# Build arm64
cmake -DCMAKE_OSX_ARCHITECTURES=arm64 ...
cmake --build .
cp output/libgodot_grpc.dylib libgodot_grpc.arm64.dylib

# Build x86_64
cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 ...
cmake --build .
cp output/libgodot_grpc.dylib libgodot_grpc.x86_64.dylib

# Combine
lipo -create libgodot_grpc.arm64.dylib libgodot_grpc.x86_64.dylib \
     -output libgodot_grpc.universal.dylib
```

## Summary

**From macOS Silicon, you can**:
- ✅ Build macOS arm64 natively
- ✅ Build macOS x86_64 via cross-compilation
- ✅ Build macOS universal binaries
- ⚠️ Build iOS (with additional setup)

**For other platforms, use**:
- GitHub Actions (recommended)
- Cloud VMs
- Docker containers (Linux only)
- Physical machines/local VMs

The GitHub Actions workflow provided handles all desktop platforms automatically, which is the recommended approach for releases.
