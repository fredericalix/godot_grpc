# godot_grpc

A production-ready **Godot 4.5+ GDExtension** that provides a native gRPC client for GDScript and C#.

[![License](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE)
[![Godot](https://img.shields.io/badge/Godot-4.5+-blue.svg)](https://godotengine.org/)
[![Platform](https://img.shields.io/badge/Platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey.svg)]()

## Overview

`godot_grpc` exposes a gRPC client to Godot, handling all transport layer concerns (HTTP/2, channels, streaming) using gRPC C++. Message serialization is kept outside the extension—you pass/receive raw bytes, allowing you to plug in any Protobuf library (we recommend [godobuf](https://github.com/oniksan/godobuf)).

### What it is
- ✅ gRPC client for unary and server-streaming RPCs
- ✅ Cross-platform: macOS (Intel + Apple Silicon), Linux x86_64, Windows x64
- ✅ Clean, minimal API exposed to GDScript/C#
- ✅ Thread-safe signal emission for async streaming
- ✅ Flexible: bring your own Protobuf serializer
- ✅ Production-ready with proper error handling and logging

### What it isn't
- ❌ No gRPC server implementation (client-only)
- ❌ No grpc-web or websocket support
- ❌ No built-in Protobuf message generation (use godobuf or similar)
- ❌ Client/bidirectional streaming not yet implemented

## Documentation

- **[Tutorial](docs/TUTORIAL.md)** - Step-by-step guide for beginners
- **[API Reference](docs/API_REFERENCE.md)** - Complete API documentation
- **[Examples](docs/EXAMPLES.md)** - Practical code examples
- **[Developer Guide](docs/DEVELOPER_GUIDE.md)** - For contributors

## Features

- **Lifecycle Management**: Connect, disconnect, check connection state
- **Unary RPCs**: Simple request-response calls
- **Server-Streaming RPCs**: Receive a stream of messages from the server
- **Signals**: `message`, `finished`, `error` for streaming events
- **Metadata**: Send custom headers with calls
- **Deadlines**: Set per-call timeouts
- **Logging**: Configurable log levels (NONE, ERROR, WARN, INFO, DEBUG, TRACE)
- **Graceful Shutdown**: Cancel in-flight calls on disconnect

## Requirements

### Build Requirements
- **CMake** 3.20+
- **C++17** compiler (AppleClang 17+, GCC 9+, MSVC 2019+)
- **vcpkg** for dependency management
- **godot-cpp** (automatically configured by build system)

### Runtime Requirements
- **Godot** 4.5 or later

### Dependencies (automatically installed via vcpkg)
- **gRPC** 1.71.0+
- **Protobuf** 5.29.5+
- **Abseil** 20250512.1+
- **RE2**, **c-ares**, **openssl** (transitive dependencies)

## Installation

### Option 1: Pre-built Binaries (Recommended)

Download the latest release from the [releases page](https://github.com/yourusername/godot_grpc/releases) and extract the `addons/godot_grpc` folder to your Godot project.

### Option 2: Build from Source

#### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/godot_grpc.git
cd godot_grpc
```

#### 2. Build the Extension

The build script will automatically set up vcpkg and install all dependencies:

**macOS/Linux:**
```bash
# Build release version (for exported games)
./scripts/build_vcpkg.sh Release

# Build debug version (for Godot editor)
./scripts/build_vcpkg.sh Debug
```

**Windows:**
```powershell
.\scripts\build_win.ps1 -BuildType Release
.\scripts\build_win.ps1 -BuildType Debug
```

The binaries will be placed in `demo/addons/godot_grpc/bin/{platform}/`.

**Build outputs:**
- **Release**: `libgodot_grpc.{platform}.template_release.{arch}.{ext}` (~17MB)
- **Debug**: `libgodot_grpc.{platform}.template_debug.{arch}.{ext}` (~65MB)

#### 3. Copy to Your Godot Project

Copy the entire addon folder to your project:

```bash
cp -r demo/addons/godot_grpc /path/to/your/project/addons/
```

#### 4. Enable in Godot

The extension is automatically enabled. Restart Godot to load it.

**Verify it's loaded:**
```gdscript
# In any script
func _ready():
    var client = GrpcClient.new()
    if client:
        print("godot_grpc loaded successfully!")
```

## Quick Start

> **New to gRPC?** Check out the **[Tutorial](docs/TUTORIAL.md)** for a step-by-step guide!

### Basic Example (Unary Call)

```gdscript
extends Node

var grpc_client: GrpcClient

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.set_log_level(3)  # INFO level

    # Connect to server
    if grpc_client.connect("dns:///localhost:50051"):
        print("Connected!")
        make_hello_call()
    else:
        print("Failed to connect")

func make_hello_call():
    # Create request (simplified protobuf encoding)
    var request = PackedByteArray()
    request.append(0x0a)  # Field 1, wire type 2 (string)
    request.append(5)     # Length
    request.append_array("Alice".to_utf8_buffer())

    # Make the call
    var response = grpc_client.unary("/helloworld.Greeter/SayHello", request)

    if response.size() > 0:
        print("Server says: ", parse_response(response))

func parse_response(data: PackedByteArray) -> String:
    # Simple protobuf parsing
    if data.size() > 2:
        var len = data[1]
        return data.slice(2, 2 + len).get_string_from_utf8()
    return ""

func _exit_tree():
    grpc_client.close()
```

**Output:**
```
Connected!
Server says: Hello, Alice! Welcome to godot_grpc demo server.
```

### Server-Streaming Example

```gdscript
extends Node

var grpc_client: GrpcClient
var stream_id: int = 0

func _ready():
    grpc_client = GrpcClient.new()

    # Connect signals for async messages
    grpc_client.message.connect(_on_stream_message)
    grpc_client.finished.connect(_on_stream_finished)
    grpc_client.error.connect(_on_stream_error)

    # Connect and start stream
    if grpc_client.connect("dns:///localhost:50051"):
        start_metrics_stream()

func start_metrics_stream():
    var request = PackedByteArray()  # Empty or add filters

    stream_id = grpc_client.server_stream_start(
        "/metrics.Monitor/StreamMetrics",
        request
    )

    if stream_id > 0:
        print("Stream started with ID: ", stream_id)

func _on_stream_message(sid: int, data: PackedByteArray):
    if sid == stream_id:
        print("Received metric data: ", data)

func _on_stream_finished(sid: int, status: int):
    if sid == stream_id:
        print("Stream completed with status: ", status)

func _on_stream_error(sid: int, code: int, msg: String):
    if sid == stream_id:
        print("Stream error: ", msg)

func _exit_tree():
    if stream_id > 0:
        grpc_client.server_stream_cancel(stream_id)
    grpc_client.close()
```

**More examples:** See [docs/EXAMPLES.md](docs/EXAMPLES.md)

## API Overview

> **Full API documentation:** See [docs/API_REFERENCE.md](docs/API_REFERENCE.md)

### Main Methods

```gdscript
# Lifecycle
grpc_client.connect(endpoint: String, options: Dictionary = {}) -> bool
grpc_client.close() -> void
grpc_client.is_connected() -> bool

# RPC Calls
grpc_client.unary(method: String, request: PackedByteArray, opts: Dictionary = {}) -> PackedByteArray
grpc_client.server_stream_start(method: String, request: PackedByteArray, opts: Dictionary = {}) -> int
grpc_client.server_stream_cancel(stream_id: int) -> void

# Logging
grpc_client.set_log_level(level: int) -> void  # 0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE
grpc_client.get_log_level() -> int
```

### Signals

```gdscript
# Emitted when a stream receives a message
grpc_client.message(stream_id: int, data: PackedByteArray)

# Emitted when a stream completes
grpc_client.finished(stream_id: int, status_code: int)

# Emitted when a stream encounters an error
grpc_client.error(stream_id: int, error_code: int, message: String)
```

### Common Options

**Connection options:**
```gdscript
{
    "keepalive_time_ms": 10000,          # Send keepalive every 10s
    "keepalive_timeout_ms": 5000,        # Wait 5s for keepalive ack
    "max_receive_message_length": 4194304,  # 4MB max
    "use_tls": 1                         # Enable TLS
}
```

**Call options:**
```gdscript
{
    "timeout_ms": 5000,                  # 5 second timeout
    "metadata": {                        # Custom headers
        "authorization": "Bearer token123"
    }
}
```

## Error Handling

gRPC status codes are mapped to Godot error codes. Error details are logged and included in signal emissions.

| gRPC Status | Godot Error |
|-------------|-------------|
| OK | OK |
| CANCELLED | ERR_QUERY_FAILED |
| UNKNOWN | ERR_BUG |
| INVALID_ARGUMENT | ERR_INVALID_PARAMETER |
| DEADLINE_EXCEEDED | ERR_TIMEOUT |
| NOT_FOUND | ERR_DOES_NOT_EXIST |
| ALREADY_EXISTS | ERR_ALREADY_EXISTS |
| PERMISSION_DENIED | ERR_UNAUTHORIZED |
| RESOURCE_EXHAUSTED | ERR_OUT_OF_MEMORY |
| FAILED_PRECONDITION | ERR_INVALID_DATA |
| ABORTED | ERR_BUSY |
| OUT_OF_RANGE | ERR_PARAMETER_RANGE_ERROR |
| UNIMPLEMENTED | ERR_UNAVAILABLE |
| INTERNAL | ERR_BUG |
| UNAVAILABLE | ERR_CANT_CONNECT |
| DATA_LOSS | ERR_FILE_CORRUPT |
| UNAUTHENTICATED | ERR_UNAUTHORIZED |

## Demo

A complete working demo is included in the `demo/` directory.

### Running the Demo

**Terminal 1: Start the demo gRPC server**
```bash
cd demo_server
make all
make run
```

You should see: `gRPC server listening on :50051`

**Terminal 2: Run the Godot demo**
```bash
cd demo
godot  # Or open in Godot Editor and press F5
```

**Expected output:**
```
Connecting to dns:///localhost:50051...
Connected successfully!
--- Demo 1: Unary Call ---
SUCCESS: Received response: Hello, Godot Player! Welcome to godot_grpc demo server.
--- Demo 2: Server Streaming ---
Stream started with ID: 1
  [Stream 1] Metric received: cpu_usage = 45.23 (timestamp: 1762084767)
  [Stream 1] Metric received: memory_usage = 78.90 (timestamp: 1762084767)
  ...
```

### What the Demo Includes

- **Unary RPC example**: `/helloworld.Greeter/SayHello`
- **Server-streaming example**: `/metrics.Monitor/StreamMetrics`
- **Signal handling**: `message`, `finished`, `error` signals
- **Simple Protobuf helpers**: Basic encoding/decoding for demonstration
- **Go server implementation**: Complete gRPC server with two services

**Demo files:**
- `demo/scripts/grpc_demo.gd` - Main demo script
- `demo_server/main.go` - gRPC server (Go)
- `demo_server/*.proto` - Protocol definitions

## Protobuf Integration

godot_grpc is **transport-only** - it handles the gRPC connection and RPC calls, but you must provide Protobuf serialization.

### Recommended Approach: Use godobuf

[godobuf](https://github.com/oniksan/godobuf) is a pure GDScript Protobuf library:

```gdscript
# 1. Generate GDScript classes from .proto files
# (using godobuf's generator)

# 2. Use in your code
var hello_request = HelloRequest.new()
hello_request.name = "Alice"
var request_bytes = hello_request.to_bytes()

var response_bytes = grpc_client.unary("/helloworld.Greeter/SayHello", request_bytes)

var hello_reply = HelloReply.new()
hello_reply.from_bytes(response_bytes)
print(hello_reply.message)
```

### Manual Serialization (for simple messages)

For quick prototyping or simple messages:

```gdscript
# Encode a string field
func encode_string_field(field_num: int, value: String) -> PackedByteArray:
    var buf = PackedByteArray()
    buf.append((field_num << 3) | 2)  # Wire type 2 = length-delimited
    buf.append(value.length())
    buf.append_array(value.to_utf8_buffer())
    return buf

# Use it
var request = encode_string_field(1, "Alice")
```

See [docs/EXAMPLES.md](docs/EXAMPLES.md) for more protobuf helpers.

## Performance Notes

- **Threading**: gRPC completion queues run on background threads. Signals are emitted on the main thread via `call_deferred()`.
- **Streaming**: Server streams spawn a dedicated reader thread per stream. Avoid creating hundreds of concurrent streams.
- **Message Size**: Default max message size is unlimited. Set limits via channel options to avoid memory issues.
- **Deadlines**: Always set reasonable deadlines to prevent hanging calls.

## TLS/Security

Basic TLS support is included but not fully configured. To enable TLS:

```gdscript
grpc_client.connect("dns:///api.example.com:443", {
    "enable_tls": true
})
```

For production use, you'll need to extend the channel options to configure:
- Root certificates
- Client certificates
- Certificate verification

This is left as an exercise for the user (POC-level security only).

## Platform-Specific Notes

### macOS
- **Architecture**: Native builds for arm64 (Apple Silicon) or x86_64 (Intel)
- **Universal binaries**: Possible but requires building for both architectures separately
- **Min version**: macOS 10.15+ (Catalina)
- **Tested on**: macOS Sequoia 15.0 (Darwin 25.0.0) with Apple M4 Max

### Linux
- **Architecture**: x86_64 (amd64)
- **Min glibc**: 2.27+
- **Tested on**: Ubuntu 20.04+, Debian 11+
- **Dependencies**: Ensure `libssl`, `libz` are installed

### Windows
- **Compiler**: Visual Studio 2019+ or MSVC 2019+
- **Architecture**: x64 only (no 32-bit support)
- **Runtime**: Links against MSVC runtime (MD)
- **Not yet tested**: Windows builds are included but untested

## Troubleshooting

> See [docs/TUTORIAL.md](docs/TUTORIAL.md#troubleshooting) for more detailed troubleshooting.

### Build Issues

**Extension won't build:**
```bash
# Clean and rebuild
rm -rf build vcpkg/buildtrees vcpkg/packages
./scripts/build_vcpkg.sh Release
```

**vcpkg errors:**
```bash
# Ensure vcpkg is set up correctly
cd vcpkg && git pull
./bootstrap-vcpkg.sh  # macOS/Linux
```

**Architecture mismatch (macOS):**
```
ld: warning: ignoring file ... wrong architecture
```
Solution: Build for your native architecture (the build script does this automatically)

### Runtime Issues

**Extension not loading:**
```
ERROR: GDExtension 'res://addons/godot_grpc/godot_grpc.gdextension' failed to load
```

**Solutions:**
1. Check architecture matches: `file demo/addons/godot_grpc/bin/macos/*.dylib`
2. Verify debug build exists for editor: `ls demo/addons/godot_grpc/bin/macos/*debug*`
3. Check file permissions: `chmod +x demo/addons/godot_grpc/bin/macos/*.dylib`
4. Restart Godot editor

**Connection fails:**
```gdscript
# Returns false
if !grpc_client.connect("dns:///localhost:50051"):
```

**Solutions:**
1. Verify server is running: `netstat -an | grep 50051`
2. Check endpoint format (must include scheme): `"dns:///localhost:50051"`
3. Try direct IP: `"ipv4:127.0.0.1:50051"`
4. Enable debug logging: `grpc_client.set_log_level(4)`

**Empty response from unary call:**
```gdscript
var response = grpc_client.unary(...)
# response.size() == 0
```

**Solutions:**
1. Check server logs for errors
2. Verify method name format: `"/package.Service/Method"`
3. Enable debug logging to see gRPC errors
4. Verify protobuf serialization is correct

**Signals not firing:**

**Solutions:**
1. Ensure signals connected **before** starting stream:
   ```gdscript
   grpc_client.message.connect(_on_message)
   var stream_id = grpc_client.server_stream_start(...)
   ```
2. Check stream started successfully: `stream_id > 0`
3. Verify server is actually sending messages (check server logs)

## Contributing

Contributions are welcome! See [docs/DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) for detailed information.

**Quick start:**
1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes with tests
4. Update documentation
5. Submit a pull request

**Areas for contribution:**
- Client-streaming and bidirectional streaming support
- Full TLS configuration
- Windows and Linux testing
- Additional examples and documentation
- Bug fixes and performance improvements

## License

This project is licensed under the BSD 3-Clause License. See [LICENSE](LICENSE) for details.

## Acknowledgments

- [Godot Engine](https://godotengine.org/)
- [gRPC](https://grpc.io/)
- [godot-cpp](https://github.com/godotengine/godot-cpp)
- [godobuf](https://github.com/oniksan/godobuf) for Protobuf integration

## Roadmap

### Planned Features
- [ ] **Client-streaming RPC** - Send multiple messages to server
- [ ] **Bidirectional streaming RPC** - Full duplex communication
- [ ] **Enhanced TLS** - Custom CA certs, client certificates, cert verification
- [ ] **Channel pooling** - Round-robin load balancing across multiple channels
- [ ] **Interceptors** - Middleware for logging, metrics, auth
- [ ] **Better error details** - Extract and expose `grpc-status-details-bin`

### Improvements
- [ ] **Windows/Linux testing** - Verify builds on Windows and Linux
- [ ] **CI/CD pipeline** - Automated builds for all platforms
- [ ] **Unit tests** - C++ unit tests with Google Test
- [ ] **Benchmarks** - Performance testing suite
- [ ] **Protobuf integration** - Optional built-in protobuf support

### Nice to Have
- [ ] **Connection retry** - Automatic reconnection with backoff
- [ ] **Health checking** - gRPC health checking protocol support
- [ ] **Metrics** - Built-in metrics collection
- [ ] **Multiple Godot versions** - Support for Godot 4.3+

## Support & Community

- **Issues**: [GitHub Issues](https://github.com/yourusername/godot_grpc/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourusername/godot_grpc/discussions)
- **Discord**: Godot Engine Discord - #gdnative channel

## Acknowledgments

Built with:
- [Godot Engine](https://godotengine.org/) - The game engine
- [gRPC C++](https://grpc.io/) - High performance RPC framework
- [godot-cpp](https://github.com/godotengine/godot-cpp) - Godot C++ bindings
- [vcpkg](https://vcpkg.io/) - C++ package management
- [godobuf](https://github.com/oniksan/godobuf) - Recommended protobuf library

Special thanks to the Godot and gRPC communities!

---

**Made with ❤️ for the Godot community**
