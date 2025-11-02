# Build script for godot_grpc using vcpkg (Windows)
param(
    [string]$BuildType = "Release"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

Write-Host "=== godot_grpc Build Script (vcpkg) ===" -ForegroundColor Green
Write-Host "Build type: $BuildType" -ForegroundColor Yellow

# Check for vcpkg
if (-not $env:VCPKG_ROOT) {
    Write-Host "Error: VCPKG_ROOT environment variable not set" -ForegroundColor Red
    Write-Host "Please install vcpkg and set VCPKG_ROOT:"
    Write-Host "  git clone https://github.com/microsoft/vcpkg.git"
    Write-Host "  cd vcpkg"
    Write-Host "  .\bootstrap-vcpkg.bat"
    Write-Host "  `$env:VCPKG_ROOT = `$pwd.Path"
    exit 1
}

Write-Host "vcpkg root: $env:VCPKG_ROOT" -ForegroundColor Yellow

# Check for godot-cpp
$GodotCppDir = Join-Path $ProjectRoot "godot-cpp"
if (-not (Test-Path $GodotCppDir)) {
    Write-Host "godot-cpp not found, cloning..." -ForegroundColor Yellow
    git clone https://github.com/godotengine/godot-cpp $GodotCppDir
    Push-Location $GodotCppDir
    git checkout 4.3
    Pop-Location
    Write-Host "godot-cpp cloned successfully" -ForegroundColor Green
}

# Note: Dependencies will be automatically installed by CMake using vcpkg.json manifest
Write-Host "Dependencies will be installed automatically via vcpkg manifest" -ForegroundColor Yellow

# Create build directory
$BuildDir = Join-Path $ProjectRoot "build"
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
Push-Location $BuildDir

# Configure
Write-Host "Configuring CMake..." -ForegroundColor Yellow
cmake .. `
    -DCMAKE_BUILD_TYPE="$BuildType" `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
    -DUSE_VCPKG=ON `
    -DGODOT_CPP_DIR="$GodotCppDir" `
    -A x64

# Build
Write-Host "Building..." -ForegroundColor Yellow
cmake --build . --config $BuildType

Pop-Location

Write-Host "=== Build completed successfully ===" -ForegroundColor Green
Write-Host "Output: $ProjectRoot\demo\addons\godot_grpc\bin\" -ForegroundColor Yellow
