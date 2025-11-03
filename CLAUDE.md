# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

`godot_grpc` is a Godot 4.5+ GDExtension that provides a native gRPC client for GDScript and C#. It's written in C++17 and uses gRPC C++ for transport, exposing a clean API to Godot for all four gRPC streaming patterns: unary, server-streaming, client-streaming, and bidirectional streaming. The extension handles only transport; Protobuf serialization is expected from external libraries like godobuf.

## Build System

### Build Commands

**macOS/Linux:**
```bash
# Prerequisites
export VCPKG_ROOT=/path/to/vcpkg  # Must be set

# Build Release (for exported games, ~17MB)
./scripts/build_vcpkg.sh Release

# Build Debug (for Godot editor, ~65MB)
./scripts/build_vcpkg.sh Debug

# Clean build
rm -rf build vcpkg/buildtrees vcpkg/packages
./scripts/build_vcpkg.sh Release
```

**Windows:**
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
.\scripts\build_win.ps1 -BuildType Release
.\scripts\build_win.ps1 -BuildType Debug
```

**Direct CMake (if needed):**
```bash
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
    -DUSE_VCPKG=ON
cmake --build . --config Release -j$(nproc)
```

### Demo Server

The demo server is a Go-based gRPC server for testing:

```bash
cd demo_server

# Generate protobuf code (if .proto files changed)
make proto

# Build and run
make all
make run

# Clean
make clean
```

### Output Structure

Binaries are placed in `demo/addons/godot_grpc/bin/{platform}/`:
- **macOS**: `libgodot_grpc.macos.template_release.arm64.dylib`
- **Linux**: `libgodot_grpc.linux.template_release.x86_64.so`
- **Windows**: `libgodot_grpc.windows.template_release.x86_64.dll`

## Code Architecture

### Core Components

1. **GrpcClient** (`src/grpc_client.{h,cpp}`)
   - Main class exposed to Godot via GDExtension
   - Manages channel lifecycle (connect, close, is_connected)
   - Handles unary RPC calls (blocking)
   - Manages all streaming RPC types: server-streaming, client-streaming, and bidirectional (async with signals)
   - New methods: `client_stream_start()`, `bidi_stream_start()`, `stream_send()`, `stream_close_send()`, `stream_cancel()`
   - Thread-safe signal emission via `call_deferred()`
   - Owns a `GrpcChannelPool` and manages multiple `GrpcStream` instances

2. **GrpcChannelPool** (`src/grpc_channel_pool.{h,cpp}`)
   - Manages gRPC channel creation and configuration
   - Converts Godot Dictionary options to gRPC ChannelArguments
   - Supports TLS, keepalive, message size limits, retry config
   - Single-channel design (one channel per connection)

3. **GrpcStream** (`src/grpc_stream.{h,cpp}`)
   - Represents a single streaming RPC call (server, client, or bidirectional)
   - Supports three stream types via `StreamType` enum
   - Spawns dedicated reader thread for all stream types
   - Spawns dedicated writer thread for client-streaming and bidirectional
   - Uses thread-safe write queue with condition variables for buffering outgoing messages
   - Emits signals (`message`, `finished`, `error`) via callback to GrpcClient
   - Handles cancellation and graceful shutdown
   - Each stream owns its own gRPC completion queue

4. **Utility: Status Mapping** (`src/util/status_map.{h,cpp}`)
   - Maps gRPC status codes to Godot Error codes
   - Ensures consistent error handling across the extension

5. **Registration** (`src/register_types.{h,cpp}`)
   - GDExtension entry point
   - Registers `GrpcClient` class with Godot

### Threading Model

- **Main thread**: Godot scripting, signal emission (via `call_deferred()`)
- **gRPC completion queue threads**: Background threads managed by gRPC for I/O (one per stream)
- **Stream reader threads**: One dedicated thread per active stream (spawned in `GrpcStream`)
- **Stream writer threads**: One dedicated thread per client-streaming or bidirectional stream (spawned in `GrpcStream`)

All signal emissions from background threads are dispatched to the main thread using `call_deferred()` to ensure thread safety. Writer threads use condition variables to efficiently wait for messages in the write queue.

### Signal Flow

1. GDScript connects to signals: `grpc_client.message.connect(_on_message)`
2. `GrpcClient::server_stream_start()` creates a `GrpcStream` with a callback
3. `GrpcStream` reader thread receives messages and invokes callback
4. Callback uses `call_deferred()` to emit signal on main thread
5. GDScript handler receives signal on main thread

### Channel Options Mapping

Godot Dictionary keys are mapped to gRPC ChannelArguments:
- `"max_retries"` → `GRPC_ARG_MAX_RECONNECT_BACKOFF_MS`
- `"keepalive_time_ms"` → `GRPC_ARG_KEEPALIVE_TIME_MS`
- `"keepalive_timeout_ms"` → `GRPC_ARG_KEEPALIVE_TIMEOUT_MS`
- `"max_receive_message_length"` → `GRPC_ARG_MAX_RECEIVE_MESSAGE_LENGTH`
- `"max_send_message_length"` → `GRPC_ARG_MAX_SEND_MESSAGE_LENGTH`
- `"use_tls"` → `grpc::InsecureChannelCredentials()` vs `grpc::SslCredentials()`

### Call Options Mapping

- `"timeout_ms"` → `ClientContext::set_deadline()`
- `"metadata"` → `ClientContext::AddMetadata(key, value)`

## Key Implementation Details

### Unary RPC Flow

1. Create `grpc::ClientContext` with timeout/metadata
2. Call `stub->Method(&context, request, &response)`
3. Check `status.ok()`, map error code
4. Return serialized response bytes to Godot

### Server-Streaming RPC Flow

1. Create `grpc::ClientContext` with timeout/metadata
2. Create `GrpcStream` object with `StreamType::SERVER_STREAMING` and callback
3. Call `stub->PrepareCall()` to get `GenericClientAsyncReaderWriter`
4. Write initial request and call `WritesDone()` immediately
5. Spawn reader thread to loop over async `Read()` operations
6. For each message, invoke callback → `call_deferred()` → emit signal
7. On completion, emit `finished` or `error` signal

### Client-Streaming RPC Flow

1. Create `grpc::ClientContext` with timeout/metadata
2. Create `GrpcStream` object with `StreamType::CLIENT_STREAMING` and callback
3. Call `stub->PrepareCall()` to get `GenericClientAsyncReaderWriter`
4. Spawn writer thread that waits on write queue
5. Spawn reader thread that waits for the single response
6. User calls `stream_send()` to queue messages → writer thread writes to gRPC
7. User calls `stream_close_send()` → writer thread calls `WritesDone()`
8. Server sends single response → reader thread receives → emit `message` signal
9. On completion, emit `finished` or `error` signal

### Bidirectional Streaming RPC Flow

1. Create `grpc::ClientContext` with timeout/metadata
2. Create `GrpcStream` object with `StreamType::BIDIRECTIONAL` and callback
3. Call `stub->PrepareCall()` to get `GenericClientAsyncReaderWriter`
4. Spawn writer thread that waits on write queue
5. Spawn reader thread that loops over async `Read()` operations
6. User calls `stream_send()` to queue messages → writer thread writes to gRPC
7. Reader thread receives messages concurrently → emit `message` signal for each
8. User calls `stream_close_send()` → writer thread calls `WritesDone()` (optional)
9. Server closes stream → reader thread completes → emit `finished` or `error` signal

### Memory Management

- `GrpcClient` is a `RefCounted` object managed by Godot
- `GrpcStream` uses `std::shared_ptr` for safe multi-threaded access
- Channel and stubs are stored as `std::shared_ptr`
- Active streams tracked in `std::map<int, std::shared_ptr<GrpcStream>>`

## Dependencies

Managed via vcpkg (manifest mode with `vcpkg.json`):
- **gRPC**: 1.71.0+
- **Protobuf**: 5.29.5+
- **Abseil**: 20250512.1+
- **RE2**, **c-ares**, **openssl**: transitive dependencies
- **godot-cpp**: 4.3 branch (cloned separately, not via vcpkg)

## Platform-Specific Notes

### macOS
- Builds for native architecture only (arm64 or x86_64)
- Universal binaries require building dependencies for both architectures in vcpkg
- Tested on: macOS Sequoia 15.0 (Darwin 25.0.0) with Apple M4 Max

### Linux
- x86_64 only
- Requires glibc 2.27+

### Windows
- x64 only (no 32-bit support)
- MSVC 2019+ required
- Not yet fully tested

## Known Limitations

1. **TLS is basic**: No custom CA cert, client cert, or cert pinning support
2. **Single channel per connection**: No connection pooling or load balancing
3. **No built-in Protobuf serialization**: By design; users must provide serialization
4. **No automatic reconnection**: User must handle connection failures manually
5. **No flow control for streaming**: Messages are queued without backpressure

## Development Workflow

### Making Changes to C++ Code

1. Edit source files in `src/`
2. Rebuild: `./scripts/build_vcpkg.sh Debug`
3. Restart Godot editor to reload extension
4. Test in `demo/` project

### Adding New Methods to GrpcClient

1. Add method declaration in `src/grpc_client.h`
2. Implement in `src/grpc_client.cpp`
3. Register with Godot in `_bind_methods()` in `grpc_client.cpp`
4. Rebuild and test

### Modifying Dependencies

1. Edit `vcpkg.json` to change versions/add packages
2. Delete `build/` and `vcpkg/buildtrees/`
3. Rebuild: `./scripts/build_vcpkg.sh Release`

### Testing with Demo Server

1. Modify `.proto` files in `demo_server/`
2. Regenerate: `cd demo_server && make proto`
3. Update `main.go` with new service logic
4. Rebuild server: `make build`
5. Run server: `make run`
6. Test with Godot demo: `cd demo && godot`

## Building for Multiple Platforms

### Local Builds from macOS Silicon

- **macOS arm64**: Native build (already working)
- **macOS x86_64**: Cross-compile on same Mac (see `docs/CROSS_COMPILE.md`)
- **macOS Universal**: Build for both architectures (see `docs/CROSS_COMPILE.md`)
- **iOS**: Experimental support with Xcode
- **Linux**: Use Docker or GitHub Actions (cross-compilation from macOS is impractical)
- **Windows**: Use GitHub Actions or Windows VM (cross-compilation from macOS is impractical)
- **Android**: Requires NDK, complex setup (not recommended for manual builds)

### GitHub Actions for Releases

The project includes `.github/workflows/build-release.yml` that automatically builds for:
- macOS (arm64 + x86_64)
- Linux (x86_64)
- Windows (x64)

To trigger a release build:
```bash
git tag v0.1.0
git push origin v0.1.0
```

See `docs/RELEASE_PROCESS.md` for complete release workflow.

### Cross-Compilation Details

- **Feasible from macOS**: macOS Intel, iOS
- **Requires native build**: Linux, Windows, Android
- **Recommended approach**: GitHub Actions with native runners per platform

Full guide: `docs/CROSS_COMPILE.md`

## Common Tasks

### Adding a New Signal

1. In `grpc_client.h`, add signal name to `_bind_methods()`:
   ```cpp
   ADD_SIGNAL(MethodInfo("new_signal", PropertyInfo(Variant::INT, "param1")));
   ```
2. Emit signal using `call_deferred()` for thread safety:
   ```cpp
   call_deferred("emit_signal", "new_signal", param1);
   ```

### Debugging Connection Issues

1. Enable debug logging in GDScript:
   ```gdscript
   grpc_client.set_log_level(4)  # DEBUG level
   ```
2. Check logs for gRPC errors
3. Use `netstat` to verify server is listening: `netstat -an | grep 50051`
4. Try direct IP endpoint: `"ipv4:127.0.0.1:50051"`

### Handling Build Errors

- **VCPKG_ROOT not set**: Export the environment variable before building
- **godot-cpp not found**: Script auto-clones to `godot-cpp/`, ensure git is available
- **Architecture mismatch**: Ensure vcpkg triplet matches target architecture
- **Linker errors**: Clean build directory and rebuild dependencies

## Code Style

- **C++ Standard**: C++17
- **Naming**: snake_case for functions/variables, PascalCase for classes
- **Indentation**: Tabs for CMake, spaces for C++ (follow godot-cpp conventions)
- **Headers**: Include guards with `#ifndef GODOT_GRPC_*_H` format
- **Godot integration**: Use `godot::` namespace, `GDCLASS` macro, `_bind_methods()`

## Security Considerations

- TLS support is POC-level; do not use in production without hardening
- No input validation on Protobuf bytes (user's responsibility)
- Signals emitted via `call_deferred()` for thread safety
- No authentication/authorization built-in (use metadata for tokens)
