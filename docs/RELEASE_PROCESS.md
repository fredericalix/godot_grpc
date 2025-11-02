# Release Process

This guide explains how to create a multi-platform release of godot_grpc.

## Prerequisites

1. GitHub repository with Actions enabled
2. Write access to create tags and releases
3. Local macOS environment for testing

## Quick Start: Automated Release via GitHub Actions

The easiest way to create a multi-platform release:

### 1. Prepare the Release

Ensure your code is ready:
```bash
# Update version in relevant files
# Update CHANGELOG.md or release notes
# Commit changes
git add .
git commit -m "Prepare v0.1.0 release"
git push origin main
```

### 2. Create and Push a Tag

```bash
# Create a tag
git tag -a v0.1.0 -m "Release version 0.1.0"

# Push the tag to GitHub
git push origin v0.1.0
```

### 3. Wait for Builds

GitHub Actions will automatically:
- ✅ Build for macOS (arm64 + x86_64) - Debug and Release
- ✅ Build for Linux (x86_64) - Debug and Release
- ✅ Build for Windows (x64) - Debug and Release
- ✅ Create a GitHub Release with all binaries

You can monitor progress at: `https://github.com/YOUR_USERNAME/godot_grpc/actions`

### 4. Verify the Release

Once complete, check `https://github.com/YOUR_USERNAME/godot_grpc/releases`:
- Download `godot_grpc-v0.1.0.zip`
- Verify it contains binaries for all platforms:
  ```
  addons/godot_grpc/
  ├── bin/
  │   ├── macos/
  │   │   ├── libgodot_grpc.macos.template_release.arm64.dylib
  │   │   └── libgodot_grpc.macos.template_release.x86_64.dylib
  │   ├── linux/
  │   │   └── libgodot_grpc.linux.template_release.x86_64.so
  │   └── windows/
  │       └── libgodot_grpc.windows.template_release.x86_64.dll
  └── godot_grpc.gdextension
  ```

### 5. Test Each Platform

Download and test on each platform:

**macOS**:
```bash
unzip godot_grpc-v0.1.0.zip
cp -r addons/godot_grpc /path/to/test/project/addons/
# Open in Godot and test
```

**Linux**: Repeat on Linux machine
**Windows**: Repeat on Windows machine

## Manual Release Process (Alternative)

If you prefer building locally or GitHub Actions fails:

### 1. Build macOS Binaries Locally

```bash
# Build universal binary (arm64 + x86_64)
./scripts/build_vcpkg.sh Release
./scripts/build_vcpkg.sh Debug
```

### 2. Build Linux Binaries

Use Docker:
```bash
docker run -it -v $(pwd):/workspace ubuntu:22.04 bash

# Inside container:
apt-get update && apt-get install -y build-essential cmake git pkg-config libssl-dev zlib1g-dev curl zip unzip tar
cd /workspace
export VCPKG_ROOT=/workspace/vcpkg_linux
git clone https://github.com/microsoft/vcpkg.git $VCPKG_ROOT
cd $VCPKG_ROOT && ./bootstrap-vcpkg.sh
cd /workspace
mkdir build_linux && cd build_linux
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -DUSE_VCPKG=ON
cmake --build . --config Release -j$(nproc)
exit
```

Copy binaries out:
```bash
cp build_linux/demo/addons/godot_grpc/bin/linux/* demo/addons/godot_grpc/bin/linux/
```

### 3. Build Windows Binaries

On a Windows machine or VM:
```powershell
git clone https://github.com/YOUR_USERNAME/godot_grpc.git
cd godot_grpc
.\scripts\build_win.ps1 -BuildType Release
.\scripts\build_win.ps1 -BuildType Debug
```

Copy binaries back to Mac via USB, network share, or cloud storage.

### 4. Package the Release

```bash
cd demo
mkdir -p ../release/addons
cp -r addons/godot_grpc ../release/addons/

# Create archive
cd ../release
zip -r godot_grpc-v0.1.0.zip addons/
```

### 5. Create GitHub Release Manually

1. Go to `https://github.com/YOUR_USERNAME/godot_grpc/releases/new`
2. Create tag: `v0.1.0`
3. Release title: `v0.1.0 - Initial Release`
4. Upload `godot_grpc-v0.1.0.zip`
5. Write release notes
6. Publish release

## Release Checklist

Before publishing:

- [ ] All binaries built successfully
- [ ] Version numbers updated in code
- [ ] CHANGELOG.md updated
- [ ] README.md reflects current features
- [ ] Documentation is up to date
- [ ] Demo project works on all platforms
- [ ] No debug logging enabled by default
- [ ] All tests pass (if applicable)
- [ ] License file included
- [ ] Clear installation instructions in release notes

## Version Numbering

Follow [Semantic Versioning](https://semver.org/):

- **MAJOR** version: Incompatible API changes
- **MINOR** version: New functionality (backwards compatible)
- **PATCH** version: Bug fixes (backwards compatible)

Examples:
- `v0.1.0` - Initial alpha release
- `v0.2.0` - Added client-streaming support
- `v0.2.1` - Fixed crash on disconnect
- `v1.0.0` - Stable production release

## Platform-Specific Notes

### macOS Binary Sizes

- **Release**: ~17MB per architecture (~34MB universal)
- **Debug**: ~65MB per architecture (~130MB universal)

For GitHub releases, include **Release builds only** unless specifically requested.

### Linux glibc Compatibility

The Linux binary built on Ubuntu 22.04 requires **glibc 2.31+**. This covers:
- ✅ Ubuntu 22.04+
- ✅ Debian 11+
- ✅ Fedora 33+
- ✅ Arch Linux (all recent)

For older distros, build on Ubuntu 20.04 (glibc 2.31) or provide static builds.

### Windows Runtime Dependencies

The Windows binary requires:
- Visual C++ Redistributable 2019+ (usually pre-installed)
- Windows 10 1809+ or Windows 11

## Troubleshooting

### GitHub Actions Build Fails

**Check logs**:
1. Go to `https://github.com/YOUR_USERNAME/godot_grpc/actions`
2. Click on failed workflow
3. Expand failed step

**Common issues**:
- **vcpkg timeout**: Dependencies take too long to build (>6 hours)
  - Solution: Add binary caching to workflow
- **godot-cpp checkout fails**: Submodule not initialized
  - Solution: Ensure `submodules: recursive` in checkout action
- **Compiler errors**: Platform-specific code issues
  - Solution: Fix code, test locally if possible

### Binary Won't Load in Godot

**macOS "damaged" error**:
```bash
xattr -cr addons/godot_grpc/bin/macos/*.dylib
```

**Linux "wrong ELF class"**:
- Verify architecture: `file addons/godot_grpc/bin/linux/*.so`
- Ensure x86_64, not i386 or arm

**Windows missing DLL**:
- Install Visual C++ Redistributable
- Check Event Viewer for specific missing DLL

## Post-Release

After publishing:

1. **Announce the release**:
   - GitHub Discussions
   - Godot Discord/Forums
   - Social media

2. **Monitor for issues**:
   - Watch GitHub Issues for bug reports
   - Test on community feedback

3. **Plan next release**:
   - Address critical bugs in patch release
   - Schedule feature additions for minor release

## Continuous Deployment (Optional)

For automated releases on every tag:

The current `.github/workflows/build-release.yml` is already configured for this.

To create nightly/beta releases, add a separate workflow:

```yaml
# .github/workflows/build-nightly.yml
on:
  schedule:
    - cron: '0 2 * * *'  # Daily at 2 AM UTC
  workflow_dispatch:

# ... same build jobs as build-release.yml
# But publish to a separate "nightly" pre-release
```

## Support and Community

- **Issues**: https://github.com/YOUR_USERNAME/godot_grpc/issues
- **Discussions**: https://github.com/YOUR_USERNAME/godot_grpc/discussions
- **Discord**: Godot Engine Discord - #gdnative channel
