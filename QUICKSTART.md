# Quick Start Guide

Get up and running with godot_grpc in 5 minutes.

## 1. Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Bootstrap (macOS/Linux)
./bootstrap-vcpkg.sh
export VCPKG_ROOT=$(pwd)

# Bootstrap (Windows)
.\bootstrap-vcpkg.bat
$env:VCPKG_ROOT = $pwd.Path
```

## 2. Build godot_grpc

```bash
# Clone the repo
git clone https://github.com/yourusername/godot_grpc.git
cd godot_grpc

# Build (macOS/Linux)
./scripts/build_vcpkg.sh Release

# Build (Windows)
.\scripts\build_win.ps1 -BuildType Release
```

This will:
- Install dependencies (gRPC, Protobuf, Abseil) via vcpkg
- Clone godot-cpp
- Build the extension
- Output to `demo/addons/godot_grpc/bin/`

## 3. Try the Demo

```bash
# Terminal 1: Start the demo server
cd demo_server
make all
make run

# Terminal 2: Open Godot demo project
cd demo
godot
```

Run the main scene and check the console output!

## 4. Use in Your Project

### Copy the Extension

```bash
cp -r demo/addons/godot_grpc /path/to/your/project/addons/
```

### Basic Usage

```gdscript
extends Node

var grpc_client: GrpcClient

func _ready():
    # Create client
    grpc_client = GrpcClient.new()

    # Connect to server
    if not grpc_client.connect("dns:///localhost:50051"):
        print("Connection failed!")
        return

    # Make a unary call
    var request = PackedByteArray([...])  # Your serialized protobuf
    var response = grpc_client.unary("/service/Method", request)

    print("Response received: ", response.size(), " bytes")

func _exit_tree():
    grpc_client.close()
```

## Next Steps

- Read the [full README](README.md) for detailed API documentation
- Check out the [demo scripts](demo/scripts/grpc_demo.gd) for examples
- Integrate [godobuf](https://github.com/oniksan/godobuf) for Protobuf serialization
- Configure TLS, metadata, and advanced options

## Troubleshooting

**"VCPKG_ROOT not set"**
→ Make sure you ran `export VCPKG_ROOT=$(pwd)` after bootstrapping vcpkg

**"godot-cpp not found"**
→ The build script should clone it automatically. If not, clone manually to `godot-cpp/`

**"Extension not loading"**
→ Check that the binary is in `addons/godot_grpc/bin/[platform]/` and matches your OS

**"Connection refused"**
→ Ensure the demo server (or your gRPC server) is running on the correct port

## Support

Questions? Open an issue on [GitHub](https://github.com/yourusername/godot_grpc/issues).
