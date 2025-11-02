# godot_grpc Build Success! 🎉

The godot_grpc GDExtension has been successfully built for your platform.

## Build Output

✅ **Extension compiled:** `demo/addons/godot_grpc/bin/macos/libgodot_grpc.macos.template_release.arm64.dylib` (17MB)

## Platform Notes

The build system detected your architecture and built for **arm64** (Apple Silicon).

**Note on Universal Binaries:** The CMakeLists.txt now builds for the native architecture only. To build universal binaries (both arm64 and x86_64), you would need to:
1. Install vcpkg packages for both architectures
2. Set `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` when running CMake

For most use cases, building for your native architecture is sufficient.

## Next Steps

### 1. Test the Demo

```bash
# Terminal 1: Start the demo gRPC server
cd demo_server
make all
make run

# Terminal 2: Open and run the Godot demo
cd demo
godot
```

The demo project is pre-configured with the extension and includes:
- Example unary RPC call to `/helloworld.Greeter/SayHello`
- Example server-streaming call to `/metrics.Monitor/StreamMetrics`
- Signal handling examples
- Simple Protobuf helpers (for demonstration)

### 2. Use in Your Own Project

Copy the addon to your project:
```bash
cp -r demo/addons/godot_grpc /path/to/your/godot/project/addons/
```

Then in your GDScript:
```gdscript
extends Node

var grpc_client: GrpcClient

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.set_log_level(3)  # INFO level

    if grpc_client.connect("dns:///localhost:50051"):
        print("Connected to gRPC server!")

        # Make a unary call
        var request = PackedByteArray([...])  # Your serialized protobuf
        var response = grpc_client.unary("/service/Method", request)

        # Start a server stream
        var stream_id = grpc_client.server_stream_start("/service/StreamMethod", request)

func _exit_tree():
    grpc_client.close()
```

### 3. Integrate Protobuf Serialization

For real applications, integrate a Protobuf library like [godobuf](https://github.com/oniksan/godobuf) to handle message serialization/deserialization.

## Build Configuration

- **Platform:** macOS (Darwin 25.0.0)
- **Architecture:** arm64 (Apple Silicon)
- **Compiler:** AppleClang 17.0.0
- **Build Type:** Release
- **C++ Standard:** C++17
- **Dependencies:** gRPC 1.71.0, Protobuf 5.29.5, Abseil 20250512.1
- **Dependency Manager:** vcpkg

## Features Implemented

✅ **Lifecycle Management**
- `connect(endpoint, options)` - Connect to gRPC server
- `close()` - Graceful shutdown
- `is_connected()` - Check connection state

✅ **Unary RPC**
- `unary(method, request_bytes, call_opts)` - Blocking unary calls
- Metadata and deadline support
- Error mapping to Godot error codes

✅ **Server-Streaming RPC**
- `server_stream_start(method, request_bytes, call_opts)` - Start stream
- `server_stream_cancel(stream_id)` - Cancel stream
- Signals: `message`, `finished`, `error`
- Thread-safe signal emission

✅ **Logging**
- `set_log_level(level)` - 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
- `get_log_level()` - Get current level

✅ **Channel Options**
- Keepalive configuration
- TLS support (basic)
- Custom authority
- Message size limits
- Retry configuration

## Known Limitations

- Client-streaming and bidirectional streaming are stubbed (not fully implemented)
- TLS configuration is basic (POC level)
- Universal binary support requires manual configuration
- No built-in Protobuf serialization (by design - bring your own)

## Troubleshooting

If you encounter issues:

1. **Extension not loading:** Ensure the `.dylib` is in the correct path and matches your architecture
2. **Connection failures:** Check server is running and endpoint format is correct (e.g., `"dns:///localhost:50051"`)
3. **Crashes:** Enable debug logging: `grpc_client.set_log_level(4)`

## Documentation

- Full API documentation: See [README.md](README.md)
- Quick start guide: See [QUICKSTART.md](QUICKSTART.md)
- Demo server: See [demo_server/README.md](demo_server/README.md)
- Contributing: See [CONTRIBUTING.md](CONTRIBUTING.md)

## License

BSD-3-Clause - See [LICENSE](LICENSE)

---

**Enjoy using godot_grpc!** For issues or questions, visit the [GitHub repository](https://github.com/yourusername/godot_grpc).
