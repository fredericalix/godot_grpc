# Platform Builds Summary

Quick reference for building godot_grpc on multiple platforms.

## TL;DR: Can I Cross-Compile Everything from Mac Silicon?

**Short answer**: No, but you have good options.

**What you CAN build from Mac Silicon**:
- ✅ macOS arm64 (native)
- ✅ macOS x86_64 (cross-compile)
- ✅ macOS Universal Binary (both in one file)
- ⚠️ iOS (experimental, requires Xcode setup)

**What you CANNOT easily cross-compile**:
- ❌ Linux (different libc, toolchain, system ABI)
- ❌ Windows (completely different toolchain and binary format)
- ❌ Android (requires NDK, complex configuration)

## Recommended Solution: GitHub Actions (Automated)

**I've created `.github/workflows/build-release.yml` for you.**

### How to Use

```bash
# 1. Commit your changes
git add .
git commit -m "Ready for release"
git push

# 2. Create and push a version tag
git tag v0.1.0
git push origin v0.1.0

# 3. GitHub automatically builds for all platforms and creates a release
# Go to: https://github.com/YOUR_USERNAME/godot_grpc/releases
```

**What it builds automatically**:
- macOS arm64 (Apple Silicon)
- macOS x86_64 (Intel)
- Linux x86_64
- Windows x64

**Time**: ~45-90 minutes (runs in parallel)
**Cost**: Free on public repos

## Option 1: Build Everything via GitHub Actions (Recommended)

### Pros:
- ✅ Zero local setup for Linux/Windows
- ✅ Consistent build environment
- ✅ Automated on every tag push
- ✅ Free for public repositories
- ✅ Creates release automatically

### Cons:
- ⚠️ Requires GitHub account with Actions enabled
- ⚠️ Takes 45-90 minutes (but runs in parallel)

### Setup:
Already done! Just push a tag:
```bash
git tag v0.1.0 && git push origin v0.1.0
```

## Option 2: Build macOS Universal Locally + GitHub Actions for Others

### Pros:
- ✅ Control over macOS build quality
- ✅ Faster iteration for macOS
- ✅ Still automated for Linux/Windows

### Cons:
- ⚠️ Need to manage two build processes

### Steps:

**1. Build macOS Universal Binary locally:**
```bash
export VCPKG_ROOT=/path/to/vcpkg
./scripts/build_universal_macos.sh Release
# Output: demo/addons/godot_grpc/bin/macos/*.universal.dylib
```

**2. Let GitHub Actions build Linux/Windows:**
```bash
git tag v0.1.0 && git push origin v0.1.0
```

**3. Download artifacts and combine:**
- Download Linux/Windows binaries from Actions artifacts
- Copy to your local `demo/addons/godot_grpc/bin/`
- Package and upload to release

## Option 3: Manual Build on Each Platform

If you have access to each platform:

### macOS (your current machine):
```bash
./scripts/build_vcpkg.sh Release
# Or for universal: ./scripts/build_universal_macos.sh Release
```

### Linux (use Docker on your Mac):
```bash
docker run -it -v $(pwd):/workspace ubuntu:22.04 bash

# Inside container:
apt-get update && apt-get install -y build-essential cmake git pkg-config libssl-dev zlib1g-dev curl zip unzip tar
cd /workspace
export VCPKG_ROOT=/workspace/vcpkg_linux
git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT
cd $VCPKG_ROOT && ./bootstrap-vcpkg.sh
cd /workspace
./scripts/build_vcpkg.sh Release
```

### Windows (requires Windows machine/VM):
```powershell
# On Windows:
git clone YOUR_REPO
cd godot_grpc
$env:VCPKG_ROOT = "C:\vcpkg"
.\scripts\build_win.ps1 -BuildType Release
```

## What About iOS and Android?

### iOS (Experimental)
- **Feasible**: Yes, from your Mac
- **Required**: Xcode, iOS SDK
- **Complexity**: Medium
- **Status**: Not fully tested in this project
- **Guide**: See `docs/CROSS_COMPILE.md`

**When to add**:
- After desktop platforms are stable
- If you need mobile support
- Requires additional CMakeLists.txt configuration

### Android (Complex)
- **Feasible**: Yes, but very complex
- **Required**: Android NDK, custom vcpkg triplets
- **Complexity**: High
- **Status**: Not implemented
- **Recommendation**: Wait for community contributions or future work

**When to add**:
- After desktop and iOS are stable
- Consider using Unity or Unreal if primarily targeting mobile

## Comparison Table

| Platform | From Mac Silicon | Recommended Method | Time | Difficulty |
|----------|------------------|-------------------|------|------------|
| macOS arm64 | ✅ Native | Local build | 10-20 min | Easy |
| macOS x86_64 | ✅ Cross-compile | Local build | 20-30 min | Easy |
| macOS Universal | ✅ Local | `build_universal_macos.sh` | 40-60 min | Medium |
| Linux x64 | ❌ No | GitHub Actions | 30-45 min | Easy (automated) |
| Windows x64 | ❌ No | GitHub Actions | 30-45 min | Easy (automated) |
| iOS | ⚠️ Possible | Local (future) | 20-30 min | Medium |
| Android | ⚠️ Very hard | Skip for now | N/A | Very Hard |

## My Recommendation for Your First Release

### Phase 1: Desktop Platforms Only
1. ✅ Use GitHub Actions for everything
2. ✅ Push a tag: `v0.1.0-alpha`
3. ✅ Get builds for macOS (both archs), Linux, Windows
4. ✅ Test on all platforms
5. ✅ Publish release

### Phase 2: Add Universal Binary (Optional)
1. Build universal binary locally
2. Replace separate macOS binaries with universal one
3. Re-release as `v0.1.1`

### Phase 3: Mobile (Future)
1. Add iOS support when needed
2. Add Android support when needed
3. Release as `v0.2.0`

## Quick Start: Your First Multi-Platform Release

```bash
# 1. Ensure .github/workflows/build-release.yml exists (already created)
git add .github/workflows/build-release.yml
git commit -m "Add CI/CD workflow for multi-platform builds"
git push

# 2. Create first release
git tag v0.1.0-alpha
git push origin v0.1.0-alpha

# 3. Monitor build at:
# https://github.com/YOUR_USERNAME/godot_grpc/actions

# 4. When complete, check release at:
# https://github.com/YOUR_USERNAME/godot_grpc/releases

# 5. Download, test, and update release notes
```

## File Sizes Reference

| Platform | Release | Debug |
|----------|---------|-------|
| macOS arm64 | 17 MB | 65 MB |
| macOS x86_64 | 17 MB | 65 MB |
| macOS Universal | 34 MB | 130 MB |
| Linux x86_64 | ~15 MB | ~60 MB |
| Windows x64 | ~20 MB | ~70 MB |

**For releases**: Ship only Release builds to reduce download size.

## Support

- Full cross-compilation guide: `docs/CROSS_COMPILE.md`
- Release process guide: `docs/RELEASE_PROCESS.md`
- Build architecture info: `CLAUDE.md`
