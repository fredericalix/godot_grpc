# Developer Guide

Guide for developers working on the godot_grpc GDExtension codebase.

## Table of Contents

- [Project Structure](#project-structure)
- [Build System](#build-system)
- [Architecture](#architecture)
- [Adding New Features](#adding-new-features)
- [Testing](#testing)
- [Debugging](#debugging)
- [Contributing](#contributing)

---

## Project Structure

```
godot_grpc/
├── src/                          # C++ source code
│   ├── grpc_client.h/cpp         # Main GrpcClient class
│   ├── grpc_stream.h/cpp         # Server streaming implementation
│   ├── grpc_channel_pool.h/cpp   # Channel management
│   ├── register_types.h/cpp      # GDExtension registration
│   └── util/
│       └── status_map.h/cpp      # Error mapping and logging
├── godot/                        # GDExtension configuration
│   └── godot_grpc.gdextension    # Extension manifest
├── demo/                         # Demo Godot project
│   ├── addons/godot_grpc/        # Extension installation location
│   ├── scripts/                  # Demo GDScript files
│   └── main.tscn                 # Demo scene
├── demo_server/                  # Demo gRPC server (Go)
│   ├── main.go                   # Server implementation
│   ├── *.proto                   # Protocol definitions
│   └── Makefile                  # Build automation
├── docs/                         # Documentation
├── scripts/                      # Build scripts
│   └── build_vcpkg.sh            # vcpkg build script
├── CMakeLists.txt                # CMake configuration
├── vcpkg.json                    # Dependency manifest
└── README.md                     # Project readme
```

---

## Build System

### Dependencies

The project uses **vcpkg** in manifest mode to manage C++ dependencies:

- **gRPC** (1.71.0+): RPC framework
- **Protobuf** (5.29.5+): Message serialization
- **Abseil** (20250512.1+): C++ standard library extensions
- **godot-cpp** (Godot 4.5+): Godot C++ bindings

### CMake Configuration

Key CMake variables:

```cmake
CMAKE_BUILD_TYPE          # Release, Debug, RelWithDebInfo
CMAKE_OSX_ARCHITECTURES   # macOS: arm64, x86_64, or "arm64;x86_64"
GODOT_PLATFORM            # macos, linux, windows
GODOT_ARCH                # arm64, x86_64
```

### Building

**Quick build (uses vcpkg script):**
```bash
./scripts/build_vcpkg.sh Release
./scripts/build_vcpkg.sh Debug
```

**Manual CMake build:**
```bash
# Configure
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build build --config Release

# Output location
ls demo/addons/godot_grpc/bin/macos/
```

**Cross-compilation (macOS universal binary):**
```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_TOOLCHAIN_FILE=vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build --config Release
```

### Build Artifacts

Binaries are placed in:
```
demo/addons/godot_grpc/bin/{platform}/libgodot_grpc.{platform}.template_{config}.{arch}.{ext}
```

Examples:
- `macos`: `libgodot_grpc.macos.template_release.arm64.dylib`
- `linux`: `libgodot_grpc.linux.template_release.x86_64.so`
- `windows`: `godot_grpc.windows.template_release.x86_64.dll`

---

## Architecture

### Overview

The extension consists of several key components:

```
┌─────────────────┐
│   GDScript      │  User code in Godot
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  GrpcClient     │  Main API (unary, streaming, lifecycle)
└────────┬────────┘
         │
         ├───────────────┐
         ▼               ▼
┌──────────────┐  ┌──────────────┐
│ChannelPool   │  │ GrpcStream   │  Internal components
└──────────────┘  └──────────────┘
         │               │
         └───────┬───────┘
                 ▼
         ┌──────────────┐
         │   gRPC C++   │  Google's gRPC library
         └──────────────┘
```

### Class Responsibilities

#### GrpcClient (src/grpc_client.h/cpp)

Main class exposed to Godot. Responsibilities:
- Connection lifecycle management
- Unary RPC calls (blocking)
- Server stream initiation
- Signal emission coordination
- Logging configuration

**Key methods:**
- `connect()`: Creates channel via `GrpcChannelPool`
- `unary()`: Synchronous unary call using async API + completion queue
- `server_stream_start()`: Creates `GrpcStream` and tracks it

#### GrpcStream (src/grpc_stream.h/cpp)

Manages individual server-streaming RPC calls. Responsibilities:
- Background thread for reading messages
- Asynchronous message reception
- Thread-safe signal emission via `call_deferred()`
- Stream lifecycle (start, cancel, cleanup)

**Threading model:**
```
[Main Thread]                [Background Thread]
     │                              │
     ├─ start() ──────────────────> │
     │                              ├─ StartCall()
     │                              ├─ Write(request)
     │                              │
     │                              ├─ Read() loop
     │  <── call_deferred(emit) ─── ├─ Got message
     │                              ├─ Read()
     │  <── call_deferred(emit) ─── ├─ Got message
     │                              │
     │  <── call_deferred(finish) ── ├─ Stream done
     ▼                              ▼
```

#### GrpcChannelPool (src/grpc_channel_pool.h/cpp)

Manages gRPC channels and stubs. Responsibilities:
- Channel creation with options
- `GenericStub` creation for method-agnostic calls
- Channel argument configuration (keepalive, TLS, etc.)

**Channel lifecycle:**
1. Create `grpc::ChannelArguments` from options dictionary
2. Create `grpc::Channel` via `grpc::CreateCustomChannel()`
3. Create `grpc::GenericStub` for generic RPC calls
4. Store channel (currently one per client, could be pooled)

#### StatusMap (src/util/status_map.h/cpp)

Utilities for error handling and logging. Responsibilities:
- Map gRPC status codes to Godot error codes
- Provide logging macros (LOG_ERROR, LOG_INFO, etc.)
- Thread-safe logging

---

## Adding New Features

### Example: Adding Client Streaming

Here's how you would add client-streaming support:

#### 1. Update GrpcClient header

```cpp
// src/grpc_client.h
class GrpcClient : public godot::RefCounted {
    GDCLASS(GrpcClient, godot::RefCounted)

public:
    // Add new method
    int64_t client_stream_start(
        const godot::String& full_method,
        const godot::Dictionary& call_opts = godot::Dictionary()
    );

    void client_stream_send(int64_t stream_id, const godot::PackedByteArray& data);
    godot::PackedByteArray client_stream_finish(int64_t stream_id);

protected:
    static void _bind_methods();

private:
    std::unordered_map<int64_t, std::unique_ptr<GrpcClientStream>> client_streams_;
};
```

#### 2. Create GrpcClientStream class

```cpp
// src/grpc_client_stream.h
class GrpcClientStream {
public:
    GrpcClientStream(/* ... */);
    ~GrpcClientStream();

    void send(const godot::PackedByteArray& data);
    godot::PackedByteArray finish();

private:
    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> stream_;
    grpc::CompletionQueue cq_;
    std::atomic<bool> active_;
    // ...
};
```

#### 3. Implement methods

```cpp
// src/grpc_client.cpp
int64_t GrpcClient::client_stream_start(
    const godot::String& full_method,
    const godot::Dictionary& call_opts
) {
    if (!channel_pool_ || !channel_pool_->has_channel()) {
        LOG_ERROR("Not connected");
        return 0;
    }

    int64_t stream_id = next_stream_id_++;
    auto stream = std::make_unique<GrpcClientStream>(
        channel_pool_->get_stub(),
        full_method.utf8().get_data(),
        call_opts
    );

    client_streams_[stream_id] = std::move(stream);
    return stream_id;
}
```

#### 4. Bind to Godot

```cpp
// src/grpc_client.cpp
void GrpcClient::_bind_methods() {
    // Existing bindings...

    ClassDB::bind_method(D_METHOD("client_stream_start", "method", "call_opts"),
        &GrpcClient::client_stream_start, DEFVAL(godot::Dictionary()));
    ClassDB::bind_method(D_METHOD("client_stream_send", "stream_id", "data"),
        &GrpcClient::client_stream_send);
    ClassDB::bind_method(D_METHOD("client_stream_finish", "stream_id"),
        &GrpcClient::client_stream_finish);
}
```

#### 5. Document and test

- Add to `docs/API_REFERENCE.md`
- Create example in `docs/EXAMPLES.md`
- Test with demo server

---

## Testing

### Unit Testing (TODO)

The project doesn't currently have unit tests, but should use:

- **Google Test** for C++ unit tests
- Test mock gRPC services
- Test error handling paths

**Example structure:**
```cpp
// tests/test_grpc_client.cpp
#include <gtest/gtest.h>
#include "grpc_client.h"

TEST(GrpcClientTest, ConnectSuccess) {
    GrpcClient client;
    EXPECT_TRUE(client.connect("dns:///localhost:50051"));
    EXPECT_TRUE(client.is_connected());
    client.close();
}
```

### Integration Testing

Use the demo project to test:

1. **Start demo server:**
   ```bash
   cd demo_server
   make all && make run
   ```

2. **Run Godot demo:**
   ```bash
   cd demo
   godot --headless --script scripts/grpc_demo.gd
   ```

3. **Verify output:**
   - Unary call succeeds
   - Server streaming receives messages
   - No crashes or errors

### Manual Testing

Test different scenarios:

**Test connection failures:**
```gdscript
# Server not running
var client = GrpcClient.new()
if !client.connect("dns:///localhost:9999"):
    print("Expected failure: server not running")
```

**Test timeouts:**
```gdscript
var response = client.unary(
    "/service/SlowMethod",
    request,
    {"timeout_ms": 100}  # Very short timeout
)
# Should timeout
```

**Test large messages:**
```gdscript
var large_data = PackedByteArray()
large_data.resize(5 * 1024 * 1024)  # 5MB
var response = client.unary("/service/LargeMethod", large_data)
```

---

## Debugging

### Enable Debug Logging

Set log level to DEBUG or TRACE:

```gdscript
grpc_client.set_log_level(4)  # DEBUG
grpc_client.set_log_level(5)  # TRACE
```

### C++ Debugging

**macOS/Linux with lldb:**
```bash
# Build with debug symbols
./scripts/build_vcpkg.sh Debug

# Run Godot under debugger
lldb -- /Applications/Godot.app/Contents/MacOS/Godot --path demo

# Set breakpoints
(lldb) b grpc_client.cpp:123
(lldb) run
```

**Add debug prints:**
```cpp
#include "util/status_map.h"

void GrpcClient::unary(...) {
    LOG_DEBUG("unary() called with method: %s", full_method.utf8().get_data());
    // ...
}
```

### Common Issues

**Issue: Extension not loading**
```
ERROR: GDExtension 'res://addons/godot_grpc/godot_grpc.gdextension' failed to load.
```

**Solution:**
- Check architecture matches (arm64 vs x86_64)
- Verify file exists at path in `.gdextension`
- Check permissions: `chmod +x *.dylib`

**Issue: Undefined symbols**
```
dlopen: undefined symbol _grpc_channel_create
```

**Solution:**
- Verify gRPC linked correctly: `otool -L libgodot_grpc.dylib` (macOS)
- Check vcpkg installed dependencies
- Rebuild: `./scripts/build_vcpkg.sh Release`

**Issue: Signals not firing**
```gdscript
# Signal handler never called
```

**Solution:**
- Verify signal connected: `client.message.connect(_on_message)`
- Check stream started: `stream_id > 0`
- Enable debug logging to see internal state

---

## Contributing

### Code Style

Follow these conventions:

**C++ Style:**
- Use `snake_case` for methods matching Godot conventions
- Use `PascalCase` for class names
- Indent with 4 spaces (no tabs)
- Max 100 characters per line
- Use `{}` even for single-line if statements

**Example:**
```cpp
void GrpcClient::unary(const godot::String& method, ...) {
    if (!is_connected()) {
        LOG_ERROR("Not connected");
        return godot::PackedByteArray();
    }

    // Implementation...
}
```

**GDScript Style:**
- Follow [GDScript style guide](https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/gdscript_styleguide.html)
- Use `snake_case` for variables and functions
- Use `PascalCase` for classes

### Pull Request Process

1. **Fork and clone**
2. **Create feature branch:** `git checkout -b feature/client-streaming`
3. **Make changes** with clear commits
4. **Test thoroughly** (integration + manual)
5. **Update documentation** (API reference, examples)
6. **Submit PR** with description of changes

### Documentation Standards

When adding features:

1. **API Reference:** Document all public methods with:
   - Parameters and types
   - Return values
   - Code examples
   - Error conditions

2. **Examples:** Add practical examples to `docs/EXAMPLES.md`

3. **Tutorial:** Update `docs/TUTORIAL.md` if it affects beginner workflow

4. **README:** Update feature list and quick start if needed

---

## Release Process

### Version Numbering

Use [Semantic Versioning](https://semver.org/):
- `MAJOR.MINOR.PATCH`
- Example: `1.0.0`, `1.1.0`, `1.1.1`

### Creating a Release

1. **Update version** in `CMakeLists.txt`:
   ```cmake
   project(godot_grpc VERSION 1.1.0)
   ```

2. **Update CHANGELOG.md** with changes

3. **Build release binaries:**
   ```bash
   ./scripts/build_vcpkg.sh Release
   # Repeat for all platforms (macOS, Linux, Windows)
   ```

4. **Test release build** with demo

5. **Tag release:**
   ```bash
   git tag -a v1.1.0 -m "Release 1.1.0"
   git push origin v1.1.0
   ```

6. **Create GitHub release** with binaries

---

## Platform-Specific Notes

### macOS

- **Universal binaries:** Build for both arm64 and x86_64
- **Code signing:** May need to sign dylib for distribution
- **Notarization:** Required for Gatekeeper on macOS

### Linux

- **GLIBC version:** Be aware of minimum GLIBC version
- **Distribution:** Consider static linking or AppImage

### Windows

- **MSVC runtime:** Link against correct runtime (MD vs MT)
- **DLL dependencies:** Check with `dumpbin /dependents`

---

## Resources

- [Godot GDExtension docs](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/index.html)
- [gRPC C++ docs](https://grpc.io/docs/languages/cpp/)
- [Protocol Buffers docs](https://protobuf.dev/)
- [vcpkg documentation](https://vcpkg.io/en/docs/README.html)
- [CMake documentation](https://cmake.org/documentation/)

---

**Questions?** Open an issue on GitHub or check the project README!
