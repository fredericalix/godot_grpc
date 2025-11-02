# godot_grpc Tutorial

A step-by-step guide to getting started with gRPC in Godot using godot_grpc.

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Installation](#installation)
3. [Your First gRPC Call](#your-first-grpc-call)
4. [Understanding Protocol Buffers](#understanding-protocol-buffers)
5. [Working with Server Streaming](#working-with-server-streaming)
6. [Error Handling](#error-handling)
7. [Production Considerations](#production-considerations)
8. [Next Steps](#next-steps)

---

## Prerequisites

Before starting, ensure you have:

- **Godot 4.5+** installed ([download](https://godotengine.org/download))
- **Basic GDScript knowledge** ([GDScript tutorial](https://docs.godotengine.org/en/stable/tutorials/scripting/gdscript/index.html))
- **A gRPC server** to connect to (we'll use the demo server, or you can use your own)
- **Understanding of RPC concepts** (optional but helpful)

---

## Installation

### Option 1: Using Pre-built Extension

1. **Download the extension** from the [releases page](https://github.com/yourusername/godot_grpc/releases)

2. **Extract to your project:**
   ```bash
   # Extract to your Godot project
   cp -r godot_grpc/addons/godot_grpc /path/to/your/project/addons/
   ```

3. **Enable the plugin** in Godot:
   - Open your project in Godot
   - Go to **Project → Project Settings → Plugins**
   - Enable **godot_grpc**

### Option 2: Building from Source

See [BUILD_SUCCESS.md](../BUILD_SUCCESS.md) for build instructions.

---

## Your First gRPC Call

Let's make your first gRPC call! We'll create a simple scene that connects to a gRPC server and makes a unary call.

### Step 1: Set Up the Demo Server

We'll use the included demo server for this tutorial.

```bash
# Terminal 1: Start the demo server
cd demo_server
make all
make run
```

You should see:
```
gRPC server listening on :50051
```

Keep this terminal running.

### Step 2: Create a New Scene

1. **Create a new scene** in Godot:
   - Create a new Node (call it `GrpcDemo`)
   - Attach a new script: `grpc_demo.gd`

2. **Save the scene** as `grpc_demo.tscn`

### Step 3: Write Your First Script

Open `grpc_demo.gd` and write:

```gdscript
extends Node

var grpc_client: GrpcClient

func _ready():
    # Create the client
    grpc_client = GrpcClient.new()

    # Enable debug logging to see what's happening
    grpc_client.set_log_level(4)

    # Connect to the server
    print("Connecting to server...")
    if grpc_client.connect("dns:///localhost:50051"):
        print("Connected!")

        # Make a simple call
        say_hello("Alice")
    else:
        print("Failed to connect")

func say_hello(name: String):
    print("Calling SayHello with name: ", name)

    # Create the request (we'll explain this next)
    var request = create_hello_request(name)

    # Make the RPC call
    var response = grpc_client.unary("/helloworld.Greeter/SayHello", request)

    # Check if we got a response
    if response.size() > 0:
        # Parse the response (we'll explain this next)
        var message = parse_hello_reply(response)
        print("Server says: ", message)
    else:
        print("Call failed")

func create_hello_request(name: String) -> PackedByteArray:
    # Protobuf encoding for HelloRequest message
    # Format: field_tag (1 byte) + length (1 byte) + data
    var buffer = PackedByteArray()
    buffer.append(0x0a)  # Tag for field 1 (wire type 2 = length-delimited)
    buffer.append(name.length())
    buffer.append_array(name.to_utf8_buffer())
    return buffer

func parse_hello_reply(data: PackedByteArray) -> String:
    # Simple protobuf parsing for HelloReply message
    # Skip tag and length, then read string
    if data.size() < 2:
        return ""

    var string_length = data[1]
    if data.size() < 2 + string_length:
        return ""

    var string_data = data.slice(2, 2 + string_length)
    return string_data.get_string_from_utf8()

func _exit_tree():
    # Always clean up!
    if grpc_client:
        grpc_client.close()
```

### Step 4: Run Your Scene

1. Press **F6** to run the scene
2. You should see:

```
Connecting to server...
[GodotGRPC INFO] Creating gRPC channel to dns:///localhost:50051
Connected!
Calling SayHello with name: Alice
[GodotGRPC INFO] Unary call to /helloworld.Greeter/SayHello
Server says: Hello, Alice! Welcome to godot_grpc demo server.
```

**Congratulations!** You just made your first gRPC call from Godot! 🎉

---

## Understanding Protocol Buffers

You probably noticed the `create_hello_request()` and `parse_hello_reply()` functions. These handle **Protocol Buffers** serialization.

### What are Protocol Buffers?

Protocol Buffers (protobuf) is Google's language-neutral way of serializing structured data. Think of it like JSON, but:
- **Much smaller** (binary format)
- **Much faster** (no parsing text)
- **Strongly typed** (defined in `.proto` files)

### The .proto Definition

The demo server uses this definition (demo_server/helloworld.proto):

```protobuf
syntax = "proto3";

package helloworld;

service Greeter {
  rpc SayHello (HelloRequest) returns (HelloReply) {}
}

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string message = 1;
}
```

This defines:
- **Service:** `Greeter` with method `SayHello`
- **Request message:** `HelloRequest` with one field: `name` (string)
- **Response message:** `HelloReply` with one field: `message` (string)

### Manual Serialization (Simple)

For simple messages, you can serialize manually:

```gdscript
func create_hello_request(name: String) -> PackedByteArray:
    var buffer = PackedByteArray()

    # Field 1: name (string)
    # Tag: (field_number << 3) | wire_type = (1 << 3) | 2 = 0x0a
    buffer.append(0x0a)
    buffer.append(name.length())
    buffer.append_array(name.to_utf8_buffer())

    return buffer
```

**Protobuf wire types:**
- `0` - Varint (int32, int64, bool)
- `1` - 64-bit (fixed64, double)
- `2` - Length-delimited (string, bytes, messages)
- `5` - 32-bit (fixed32, float)

**Tag calculation:** `(field_number << 3) | wire_type`

### Using a Protobuf Library (Recommended)

For complex messages, use a library like [godobuf](https://github.com/oniksan/godobuf):

```gdscript
# With godobuf (pseudocode)
var hello_request = HelloRequest.new()
hello_request.name = "Alice"
var request_bytes = hello_request.serialize()

var response_bytes = grpc_client.unary("/helloworld.Greeter/SayHello", request_bytes)
var hello_reply = HelloReply.deserialize(response_bytes)
print(hello_reply.message)
```

---

## Working with Server Streaming

Server streaming allows the server to send multiple messages in response to one request. Let's stream metrics from the server!

### Step 1: Create a Streaming Scene

Create a new script called `streaming_demo.gd`:

```gdscript
extends Node

var grpc_client: GrpcClient
var active_stream_id: int = 0

func _ready():
    # Create and configure client
    grpc_client = GrpcClient.new()
    grpc_client.set_log_level(3)  # INFO level

    # Connect signals
    grpc_client.message.connect(_on_stream_message)
    grpc_client.finished.connect(_on_stream_finished)
    grpc_client.error.connect(_on_stream_error)

    # Connect to server
    if grpc_client.connect("dns:///localhost:50051"):
        print("Connected! Starting metrics stream...")
        start_metrics_stream()
    else:
        print("Failed to connect")

func start_metrics_stream():
    # Create request: 500ms interval, 10 messages
    var request = create_metrics_request(500, 10)

    # Start the stream
    active_stream_id = grpc_client.server_stream_start(
        "/metrics.Monitor/StreamMetrics",
        request
    )

    if active_stream_id > 0:
        print("Stream started with ID: ", active_stream_id)
    else:
        print("Failed to start stream")

func create_metrics_request(interval_ms: int, count: int) -> PackedByteArray:
    var buffer = PackedByteArray()

    # Field 1: interval_ms (int32)
    buffer.append(0x08)  # Tag: (1 << 3) | 0 = 8
    buffer.append_array(encode_varint(interval_ms))

    # Field 2: count (int32)
    buffer.append(0x10)  # Tag: (2 << 3) | 0 = 16
    buffer.append_array(encode_varint(count))

    return buffer

func encode_varint(value: int) -> PackedByteArray:
    var buffer = PackedByteArray()
    while value > 127:
        buffer.append((value & 0x7F) | 0x80)
        value >>= 7
    buffer.append(value & 0x7F)
    return buffer

func _on_stream_message(stream_id: int, data: PackedByteArray):
    if stream_id != active_stream_id:
        return

    # Parse the metric data
    var metric = parse_metric_data(data)
    print("[Stream %d] %s = %.2f" % [stream_id, metric.name, metric.value])

func _on_stream_finished(stream_id: int, status_code: int):
    print("[Stream %d] Finished with status: %d" % [stream_id, status_code])
    if status_code == 0:
        print("Stream completed successfully!")

func _on_stream_error(stream_id: int, error_code: int, message: String):
    print("[Stream %d] Error: %s (code: %d)" % [stream_id, message, error_code])

func parse_metric_data(data: PackedByteArray) -> Dictionary:
    # Simple parser for MetricData message
    var result = {"name": "", "value": 0.0, "timestamp": 0}

    var i = 0
    while i < data.size():
        var tag = data[i]
        i += 1

        var field_number = tag >> 3
        var wire_type = tag & 0x07

        if field_number == 1:  # name (string)
            var length = data[i]
            i += 1
            var name_bytes = data.slice(i, i + length)
            result.name = name_bytes.get_string_from_utf8()
            i += length
        elif field_number == 2:  # value (double)
            var value_bytes = data.slice(i, i + 8)
            result.value = value_bytes.decode_double(0)
            i += 8
        elif field_number == 3:  # timestamp (int64)
            var ts = decode_varint(data, i)
            result.timestamp = ts.value
            i = ts.new_index

    return result

func decode_varint(data: PackedByteArray, start: int) -> Dictionary:
    var value = 0
    var shift = 0
    var i = start

    while i < data.size():
        var byte = data[i]
        value |= (byte & 0x7F) << shift
        i += 1
        if (byte & 0x80) == 0:
            break
        shift += 7

    return {"value": value, "new_index": i}

func _exit_tree():
    if grpc_client:
        grpc_client.close()
```

### Step 2: Run the Streaming Demo

Run the scene and you should see:

```
Connected! Starting metrics stream...
Stream started with ID: 1
[GodotGRPC INFO] Server stream 1 started
[Stream 1] cpu_usage = 45.23
[Stream 1] memory_usage = 78.90
[Stream 1] disk_io = 12.34
...
[Stream 1] Finished with status: 0
Stream completed successfully!
```

### Understanding Signals

The streaming demo uses three signals:

1. **message(stream_id, data)** - Called for each message received
2. **finished(stream_id, status_code)** - Called when stream completes
3. **error(stream_id, error_code, message)** - Called on error

**Important:** Signals are emitted on the main thread, so you can safely:
- Update UI elements
- Modify scene tree
- Call other Godot functions

---

## Working with Client-Streaming and Bidirectional Streaming

godot_grpc now supports all four gRPC streaming patterns! Let's explore client-streaming and bidirectional streaming.

### Client-Streaming: Send Many, Receive One

Client-streaming allows you to send multiple messages and receive a single response. This is perfect for uploading files in chunks or batching data.

```gdscript
extends Node

var grpc_client: GrpcClient
var stream_id: int = 0

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_response)
    grpc_client.finished.connect(_on_finished)

    if grpc_client.connect("dns:///localhost:50051"):
        upload_file()

func upload_file():
    # Start client-streaming
    stream_id = grpc_client.client_stream_start("/upload.FileService/UploadFile", {})

    if stream_id <= 0:
        print("Failed to start upload")
        return

    print("Uploading file in chunks...")

    # Simulate sending 5 chunks
    for chunk_num in range(5):
        var chunk = create_chunk(chunk_num, "data_chunk_%d" % chunk_num)
        if !grpc_client.stream_send(stream_id, chunk):
            print("Failed to send chunk ", chunk_num)
            return

        print("Sent chunk ", chunk_num)
        await get_tree().create_timer(0.1).timeout

    # Signal we're done sending
    grpc_client.stream_close_send(stream_id)
    print("Upload complete, waiting for server confirmation...")

func create_chunk(number: int, data: String) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: chunk_number (int32)
    buf.append(0x08)
    buf.append(number)
    # Field 2: data (string)
    buf.append(0x12)
    buf.append(data.length())
    buf.append_array(data.to_utf8_buffer())
    return buf

func _on_response(sid: int, data: PackedByteArray):
    if sid == stream_id:
        print("Server response: Upload successful!")
        var response = parse_response(data)
        print("File ID: ", response.file_id)

func _on_finished(sid: int, status: int):
    if sid == stream_id:
        print("Upload stream finished with status: ", status)

func parse_response(data: PackedByteArray) -> Dictionary:
    # Parse the single response from server
    return {"file_id": "uploaded_file_123", "size": 1024}

func _exit_tree():
    grpc_client.close()
```

### Bidirectional Streaming: Full Duplex Communication

Bidirectional streaming allows both client and server to send messages concurrently. Perfect for real-time chat, game state sync, or interactive sessions.

```gdscript
extends Node

var grpc_client: GrpcClient
var stream_id: int = 0
var message_count: int = 0

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_server_message)
    grpc_client.finished.connect(_on_stream_finished)
    grpc_client.error.connect(_on_stream_error)

    if grpc_client.connect("dns:///localhost:50051"):
        start_chat()

func start_chat():
    # Start bidirectional stream
    stream_id = grpc_client.bidi_stream_start("/chat.Chat/StreamChat", {})

    if stream_id <= 0:
        print("Failed to start chat")
        return

    print("Chat started!")

    # Send initial message
    send_chat_message("Hello from Godot!")

    # Send periodic messages
    for i in range(3):
        await get_tree().create_timer(2.0).timeout
        send_chat_message("Message %d" % (i + 1))

    # Optionally close our send side (server can still send)
    await get_tree().create_timer(1.0).timeout
    grpc_client.stream_close_send(stream_id)
    print("Stopped sending messages (but still receiving)")

func send_chat_message(text: String):
    var message = create_chat_message(text)
    if grpc_client.stream_send(stream_id, message):
        print("Sent: ", text)
    else:
        print("Failed to send: ", text)

func create_chat_message(text: String) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: sender (string)
    buf.append(0x0a)
    var sender = "GodotPlayer"
    buf.append(sender.length())
    buf.append_array(sender.to_utf8_buffer())
    # Field 2: text (string)
    buf.append(0x12)
    buf.append(text.length())
    buf.append_array(text.to_utf8_buffer())
    return buf

func _on_server_message(sid: int, data: PackedByteArray):
    if sid != stream_id:
        return

    var msg = parse_chat_message(data)
    print("[%s]: %s" % [msg.sender, msg.text])

    # Can respond to server messages
    message_count += 1
    if message_count == 2:
        send_chat_message("Thanks for the updates!")

func _on_stream_finished(sid: int, status: int):
    if sid == stream_id:
        print("Chat stream ended")

func _on_stream_error(sid: int, code: int, message: String):
    if sid == stream_id:
        print("Chat error: ", message)

func parse_chat_message(data: PackedByteArray) -> Dictionary:
    # Parse incoming messages
    return {"sender": "Server", "text": "Message from server", "timestamp": 0}

func _exit_tree():
    if stream_id > 0:
        grpc_client.stream_cancel(stream_id)
    grpc_client.close()
```

### Key Differences Between Stream Types

| Stream Type | Client Sends | Server Sends | Use Cases |
|-------------|--------------|--------------|-----------|
| **Unary** | One | One | Simple requests, queries |
| **Server-streaming** | One | Many | Live updates, downloads |
| **Client-streaming** | Many | One | Uploads, batching data |
| **Bidirectional** | Many | Many | Chat, real-time sync, interactive sessions |

### When to Use Each Pattern

**Use Client-Streaming when:**
- Uploading files in chunks
- Batching analytics events
- Sending sensor data in bursts
- Submitting form data incrementally

**Use Bidirectional Streaming when:**
- Real-time chat or messaging
- Live game state synchronization
- Interactive command/response sessions
- Collaborative editing

---

## Error Handling

Robust error handling is crucial for production applications.

### Connection Errors

```gdscript
func try_connect():
    if !grpc_client.connect("dns:///api.example.com:50051"):
        print("Connection failed - server may be down")
        show_error_dialog("Unable to connect to server")
        return false
    return true
```

### Call Timeouts

```gdscript
func make_call_with_timeout():
    var call_opts = {
        "timeout_ms": 5000  # 5 second timeout
    }

    var response = grpc_client.unary(
        "/service/Method",
        request,
        call_opts
    )

    if response.size() == 0:
        print("Call failed or timed out")
        return null

    return parse_response(response)
```

### Stream Error Handling

```gdscript
func _on_stream_error(stream_id: int, error_code: int, message: String):
    match error_code:
        ERR_TIMEOUT:
            print("Stream timed out - retrying...")
            retry_stream()

        ERR_UNAVAILABLE:
            print("Server unavailable - will retry in 5s")
            await get_tree().create_timer(5.0).timeout
            retry_stream()

        ERR_UNAUTHORIZED:
            print("Authentication failed")
            show_login_dialog()

        _:
            print("Unknown error: ", message)
```

### Checking Connection Status

```gdscript
func _process(delta):
    if grpc_client and !grpc_client.is_connected():
        print("Lost connection to server!")
        attempt_reconnect()
```

---

## Production Considerations

### Authentication

Add authentication headers to your calls:

```gdscript
func authenticated_call(method: String, request: PackedByteArray):
    var token = get_auth_token()  # Get from your auth system

    var call_opts = {
        "metadata": {
            "authorization": "Bearer " + token
        }
    }

    return grpc_client.unary(method, request, call_opts)
```

### Connection Pooling

Reuse a single client across your game:

```gdscript
# autoload/grpc_manager.gd (singleton)
extends Node

var client: GrpcClient

func _ready():
    client = GrpcClient.new()
    client.set_log_level(2)  # WARN level in production

    # Connect with production settings
    var opts = {
        "use_tls": 1,
        "keepalive_time_ms": 30000,
        "keepalive_timeout_ms": 10000
    }

    if !client.connect("dns:///api.production.com:443", opts):
        push_error("Failed to connect to game server")

func _exit_tree():
    client.close()

# Use from other scripts:
# GrpcManager.client.unary(...)
```

### Resource Management

Always clean up resources:

```gdscript
class_name GameClient
extends RefCounted

var grpc_client: GrpcClient
var active_streams: Array[int] = []

func _init():
    grpc_client = GrpcClient.new()

func start_stream(method: String, request: PackedByteArray) -> int:
    var stream_id = grpc_client.server_stream_start(method, request)
    if stream_id > 0:
        active_streams.append(stream_id)
    return stream_id

func cleanup():
    # Cancel all active streams
    for stream_id in active_streams:
        grpc_client.server_stream_cancel(stream_id)
    active_streams.clear()

    # Close connection
    grpc_client.close()
```

### Logging Configuration

Different log levels for different environments:

```gdscript
func configure_logging():
    if OS.is_debug_build():
        grpc_client.set_log_level(4)  # DEBUG
    else:
        grpc_client.set_log_level(2)  # WARN
```

### TLS/SSL Configuration

For production, always use TLS:

```gdscript
func connect_secure(endpoint: String):
    var opts = {
        "use_tls": 1,
        # Optional: provide custom CA certificate
        # "tls_cert_path": "res://certs/ca.pem"
    }

    return grpc_client.connect(endpoint, opts)
```

---

## Next Steps

Now that you understand the basics, here's where to go next:

### 1. Integrate Protobuf Serialization

Stop writing manual serialization and use a proper library:

- **godobuf**: Pure GDScript protobuf library ([GitHub](https://github.com/oniksan/godobuf))
- Write your own GDExtension wrapper around protobuf C++

### 2. Build Your Own Service

Create your own gRPC service:

1. **Define your .proto file**
2. **Generate server code** (Python, Go, Node.js, etc.)
3. **Implement your service**
4. **Connect from Godot**

Example .proto for a game backend:

```protobuf
syntax = "proto3";

package game;

service GameBackend {
  rpc Login (LoginRequest) returns (LoginResponse) {}
  rpc GetPlayerData (PlayerRequest) returns (PlayerData) {}
  rpc UpdateScore (ScoreUpdate) returns (ScoreResponse) {}
  rpc MatchmakingStream (MatchRequest) returns (stream MatchUpdate) {}
}

message LoginRequest {
  string username = 1;
  string password = 2;
}

message LoginResponse {
  bool success = 1;
  string token = 2;
  string player_id = 3;
}
```

### 3. Explore Advanced Features

- **Metadata and headers**: Send custom headers with requests
- **Deadlines and timeouts**: Set per-call timeouts
- **Error handling patterns**: Implement retry logic
- **Connection management**: Handle reconnections gracefully

### 4. Read the Documentation

- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Examples](EXAMPLES.md) - More code examples
- [Developer Guide](DEVELOPER_GUIDE.md) - For contributing to godot_grpc

---

## Troubleshooting

### Extension not loading

**Problem:**
```
ERROR: GDExtension 'res://addons/godot_grpc/godot_grpc.gdextension' failed to load.
```

**Solutions:**
1. Check file exists at the path specified in `.gdextension`
2. Verify architecture matches (arm64 vs x86_64)
3. Check file permissions: `chmod +x *.dylib` (macOS/Linux)
4. Ensure you have the correct build (debug for editor, release for export)

### Connection refused

**Problem:**
```
Failed to connect
```

**Solutions:**
1. Verify server is running: `netstat -an | grep 50051`
2. Check endpoint format: `"dns:///localhost:50051"`
3. Verify firewall settings
4. Try direct IP: `"ipv4:127.0.0.1:50051"`

### Empty response from unary call

**Problem:**
```gdscript
var response = grpc_client.unary(...)
# response.size() == 0
```

**Solutions:**
1. Enable debug logging: `grpc_client.set_log_level(4)`
2. Check server logs for errors
3. Verify protobuf serialization is correct
4. Check method name format: `"/package.Service/Method"`

### Signals not firing

**Problem:**
```gdscript
# _on_stream_message never called
```

**Solutions:**
1. Verify signal connected: `grpc_client.message.connect(_on_stream_message)`
2. Check stream started: `stream_id > 0`
3. Enable logging to see internal state
4. Verify server is actually sending messages

---

## Getting Help

- **GitHub Issues**: [Report bugs or ask questions](https://github.com/yourusername/godot_grpc/issues)
- **Godot Discord**: #gdnative channel
- **Documentation**: Check [API_REFERENCE.md](API_REFERENCE.md) for detailed API docs

---

**Happy coding!** You're now ready to build networked Godot games with gRPC! 🚀
