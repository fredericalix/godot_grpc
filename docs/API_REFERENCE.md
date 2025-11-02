# API Reference

Complete API documentation for the godot_grpc GDExtension.

## Table of Contents

- [GrpcClient Class](#grpcclient-class)
  - [Properties](#properties)
  - [Methods](#methods)
  - [Signals](#signals)
  - [Constants](#constants)
- [Data Types](#data-types)
- [Error Codes](#error-codes)
- [Channel Options](#channel-options)
- [Call Options](#call-options)

---

## GrpcClient Class

The main class for interacting with gRPC servers. Handles connection lifecycle, unary calls, and streaming RPCs.

### Properties

None. The `GrpcClient` maintains internal state but does not expose properties directly.

### Methods

#### Lifecycle Management

##### `connect(endpoint: String, options: Dictionary = {}) -> bool`

Connects to a gRPC server.

**Parameters:**
- `endpoint` (String): The server endpoint in gRPC format
  - `"dns:///hostname:port"` - DNS resolver (recommended)
  - `"ipv4:127.0.0.1:50051"` - Direct IPv4
  - `"ipv6:[::1]:50051"` - Direct IPv6
  - `"unix:///path/to/socket"` - Unix domain socket
- `options` (Dictionary, optional): Channel configuration options (see [Channel Options](#channel-options))

**Returns:** `bool` - `true` if connection successful, `false` otherwise

**Example:**
```gdscript
var client = GrpcClient.new()

# Basic connection
if client.connect("dns:///localhost:50051"):
    print("Connected!")

# Connection with options
var opts = {
    "keepalive_time_ms": 10000,
    "keepalive_timeout_ms": 5000,
    "max_receive_message_length": 4194304
}
if client.connect("dns:///api.example.com:443", opts):
    print("Connected with custom options!")
```

---

##### `close() -> void`

Closes the connection and releases all resources. Should be called when done with the client.

**Example:**
```gdscript
func _exit_tree():
    grpc_client.close()
```

---

##### `is_connected() -> bool`

Checks if the client is currently connected to a server.

**Returns:** `bool` - `true` if connected, `false` otherwise

**Example:**
```gdscript
if client.is_connected():
    print("Client is connected")
else:
    print("Client is disconnected")
```

---

#### RPC Methods

##### `unary(method: String, request_bytes: PackedByteArray, call_opts: Dictionary = {}) -> PackedByteArray`

Makes a unary (request-response) RPC call. This is a **blocking** call.

**Parameters:**
- `method` (String): Full method path in format `"/package.Service/Method"`
  - Example: `"/helloworld.Greeter/SayHello"`
- `request_bytes` (PackedByteArray): Serialized protobuf request message
- `call_opts` (Dictionary, optional): Per-call options (see [Call Options](#call-options))

**Returns:** `PackedByteArray` - Serialized protobuf response message (empty on error)

**Example:**
```gdscript
# Prepare request (you need a protobuf serializer)
var request = serialize_hello_request("Alice")

# Make unary call
var response_bytes = client.unary("/helloworld.Greeter/SayHello", request)

if response_bytes.size() > 0:
    var response = deserialize_hello_reply(response_bytes)
    print("Received: ", response.message)
else:
    print("Call failed")
```

**With Call Options:**
```gdscript
var call_opts = {
    "timeout_ms": 5000,
    "metadata": {
        "authorization": "Bearer token123",
        "x-request-id": "req-001"
    }
}

var response = client.unary("/api.Service/Method", request, call_opts)
```

---

##### `server_stream_start(method: String, request_bytes: PackedByteArray, call_opts: Dictionary = {}) -> int`

Starts a server-streaming RPC call. Messages are received via signals.

**Parameters:**
- `method` (String): Full method path in format `"/package.Service/Method"`
- `request_bytes` (PackedByteArray): Serialized protobuf request message
- `call_opts` (Dictionary, optional): Per-call options

**Returns:** `int` - Stream ID (> 0 on success, 0 on failure)

**Example:**
```gdscript
func start_metrics_stream():
    var request = serialize_metrics_request(100, 10)  # 100ms interval, 10 messages
    var stream_id = client.server_stream_start("/metrics.Monitor/StreamMetrics", request)

    if stream_id > 0:
        print("Stream started with ID: ", stream_id)
    else:
        print("Failed to start stream")
```

---

##### `server_stream_cancel(stream_id: int) -> void`

Cancels an active server stream.

**Parameters:**
- `stream_id` (int): The stream ID returned by `server_stream_start()`

**Example:**
```gdscript
var stream_id = client.server_stream_start("/service/StreamMethod", request)

# Later, cancel the stream
client.server_stream_cancel(stream_id)
```

---

#### Logging

##### `set_log_level(level: int) -> void`

Sets the logging verbosity level.

**Parameters:**
- `level` (int): Log level (use constants below)
  - `0` - NONE: No logging
  - `1` - ERROR: Errors only
  - `2` - WARN: Warnings and errors
  - `3` - INFO: Informational messages (default)
  - `4` - DEBUG: Detailed debug information
  - `5` - TRACE: Very verbose tracing

**Example:**
```gdscript
client.set_log_level(4)  # Enable debug logging
```

---

##### `get_log_level() -> int`

Gets the current logging level.

**Returns:** `int` - Current log level (0-5)

**Example:**
```gdscript
var current_level = client.get_log_level()
print("Log level: ", current_level)
```

---

### Signals

#### `message(stream_id: int, data: PackedByteArray)`

Emitted when a message is received on a server stream.

**Parameters:**
- `stream_id` (int): The ID of the stream that received the message
- `data` (PackedByteArray): Serialized protobuf message

**Example:**
```gdscript
func _ready():
    client.message.connect(_on_stream_message)

func _on_stream_message(stream_id: int, data: PackedByteArray):
    print("Stream ", stream_id, " received message")
    var message = deserialize_metric_data(data)
    print("Metric: ", message.name, " = ", message.value)
```

---

#### `finished(stream_id: int, status_code: int)`

Emitted when a server stream completes (successfully or with error).

**Parameters:**
- `stream_id` (int): The ID of the stream that finished
- `status_code` (int): gRPC status code (0 = OK, see [Error Codes](#error-codes))

**Example:**
```gdscript
func _ready():
    client.finished.connect(_on_stream_finished)

func _on_stream_finished(stream_id: int, status_code: int):
    if status_code == 0:
        print("Stream ", stream_id, " completed successfully")
    else:
        print("Stream ", stream_id, " finished with error: ", status_code)
```

---

#### `error(stream_id: int, error_code: int, message: String)`

Emitted when an error occurs on a server stream.

**Parameters:**
- `stream_id` (int): The ID of the stream with the error
- `error_code` (int): Godot error code (see Godot's @GlobalScope Error enum)
- `message` (String): Human-readable error message

**Example:**
```gdscript
func _ready():
    client.error.connect(_on_stream_error)

func _on_stream_error(stream_id: int, error_code: int, message: String):
    print("Stream ", stream_id, " error: ", message, " (code: ", error_code, ")")
```

---

### Constants

The extension uses Godot's built-in error constants (`@GlobalScope.Error`):

- `OK` (0) - Success
- `ERR_UNAVAILABLE` - Server unavailable
- `ERR_TIMEOUT` - Operation timed out
- `ERR_UNAUTHORIZED` - Authentication failed
- `ERR_INVALID_PARAMETER` - Invalid argument
- `ERR_CONNECTION_ERROR` - Connection failed
- And other standard Godot error codes

---

## Data Types

### PackedByteArray

All protobuf messages are passed as `PackedByteArray`. You must use a protobuf serialization library to convert between GDScript objects and bytes.

**Recommended libraries:**
- [godobuf](https://github.com/oniksan/godobuf) - Pure GDScript protobuf library
- Custom GDNative/GDExtension protobuf wrapper

---

## Error Codes

gRPC status codes are mapped to Godot error codes:

| gRPC Status | Godot Error | Description |
|-------------|-------------|-------------|
| OK (0) | OK | Success |
| CANCELLED (1) | ERR_UNAVAILABLE | Operation cancelled |
| UNKNOWN (2) | ERR_BUG | Unknown error |
| INVALID_ARGUMENT (3) | ERR_INVALID_PARAMETER | Invalid argument |
| DEADLINE_EXCEEDED (4) | ERR_TIMEOUT | Deadline exceeded |
| NOT_FOUND (5) | ERR_DOES_NOT_EXIST | Not found |
| ALREADY_EXISTS (6) | ERR_ALREADY_EXISTS | Already exists |
| PERMISSION_DENIED (7) | ERR_UNAUTHORIZED | Permission denied |
| RESOURCE_EXHAUSTED (8) | ERR_OUT_OF_MEMORY | Resource exhausted |
| FAILED_PRECONDITION (9) | ERR_UNCONFIGURED | Precondition failed |
| ABORTED (10) | ERR_UNAVAILABLE | Operation aborted |
| OUT_OF_RANGE (11) | ERR_INVALID_PARAMETER | Out of range |
| UNIMPLEMENTED (12) | ERR_UNAVAILABLE | Not implemented |
| INTERNAL (13) | ERR_BUG | Internal error |
| UNAVAILABLE (14) | ERR_UNAVAILABLE | Service unavailable |
| DATA_LOSS (15) | ERR_FILE_CORRUPT | Data loss |
| UNAUTHENTICATED (16) | ERR_UNAUTHORIZED | Not authenticated |

---

## Channel Options

Options passed to `connect()` for configuring the gRPC channel:

```gdscript
var options = {
    # Keepalive settings (in milliseconds)
    "keepalive_time_ms": 10000,          # Send keepalive ping every 10s
    "keepalive_timeout_ms": 5000,        # Wait 5s for keepalive ack
    "keepalive_permit_without_calls": 1, # 1 = allow keepalive without active calls

    # Message size limits (in bytes)
    "max_send_message_length": 4194304,    # 4MB max send
    "max_receive_message_length": 4194304, # 4MB max receive

    # TLS settings
    "use_tls": 1,                         # 1 = enable TLS, 0 = plaintext
    "tls_cert_path": "/path/to/cert.pem", # Path to server cert (optional)

    # Other settings
    "max_reconnect_backoff_ms": 120000,   # Max backoff between reconnects
    "initial_reconnect_backoff_ms": 1000, # Initial backoff
}
```

**Common Configurations:**

**Development (localhost, no TLS):**
```gdscript
client.connect("dns:///localhost:50051")
```

**Production (with TLS):**
```gdscript
var opts = {
    "use_tls": 1,
    "keepalive_time_ms": 30000,
    "keepalive_timeout_ms": 10000
}
client.connect("dns:///api.production.com:443", opts)
```

**High-throughput (large messages):**
```gdscript
var opts = {
    "max_send_message_length": 16777216,    # 16MB
    "max_receive_message_length": 16777216  # 16MB
}
client.connect("dns:///localhost:50051", opts)
```

---

## Call Options

Per-call options passed to `unary()` or `server_stream_start()`:

```gdscript
var call_opts = {
    # Timeout for the call (in milliseconds)
    "timeout_ms": 5000,

    # Metadata (headers) to send with the call
    "metadata": {
        "authorization": "Bearer YOUR_TOKEN",
        "x-request-id": "unique-request-id",
        "x-user-id": "user123"
    }
}
```

**Example with timeout:**
```gdscript
var response = client.unary(
    "/api.Service/SlowMethod",
    request,
    {"timeout_ms": 30000}  # 30 second timeout
)
```

**Example with authentication:**
```gdscript
var call_opts = {
    "metadata": {
        "authorization": "Bearer " + get_auth_token()
    }
}
var response = client.unary("/api.Service/SecureMethod", request, call_opts)
```

---

## Best Practices

### 1. Resource Management

Always call `close()` when done:

```gdscript
var grpc_client: GrpcClient

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.connect("dns:///localhost:50051")

func _exit_tree():
    grpc_client.close()
```

### 2. Error Handling

Check return values and handle errors:

```gdscript
var response = client.unary("/service/Method", request)
if response.size() == 0:
    print("RPC failed - check logs")
    return

# Process response
var result = deserialize(response)
```

### 3. Signal Connection

Connect signals in `_ready()`:

```gdscript
func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_message)
    grpc_client.finished.connect(_on_finished)
    grpc_client.error.connect(_on_error)
```

### 4. Logging

Enable debug logging during development:

```gdscript
func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.set_log_level(4)  # DEBUG level
```

Set to INFO or WARN in production:

```gdscript
grpc_client.set_log_level(2)  # WARN level
```

---

## Thread Safety

The `GrpcClient` is **thread-safe** for:
- Calling methods from the main thread
- Receiving signals on the main thread

All signals are automatically emitted on Godot's main thread using `call_deferred()`, so you can safely interact with Godot's scene tree and UI from signal handlers.

---

## Limitations

1. **Client-only**: No server implementation
2. **No client streaming**: Only unary and server-streaming supported
3. **No bidirectional streaming**: Not implemented
4. **Blocking unary calls**: `unary()` blocks the calling thread
5. **No built-in protobuf**: You must provide your own serialization

For client/bidirectional streaming, consider using multiple server streams or contributing to the project!
