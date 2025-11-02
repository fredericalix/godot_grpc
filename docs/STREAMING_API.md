# Client-Streaming and Bidirectional Streaming API

This document describes the client-streaming and bidirectional streaming functionality added to the godot_grpc extension.

## Overview

The extension now supports all four gRPC streaming patterns:

1. **Unary**: Single request → Single response (already implemented)
2. **Server-streaming**: Single request → Multiple responses (already implemented)
3. **Client-streaming**: Multiple requests → Single response (NEW)
4. **Bidirectional streaming**: Multiple requests → Multiple responses (NEW)

## API Reference

### Client-Streaming RPC

Start a client-streaming RPC where the client sends multiple messages and receives one response.

```gdscript
# Start the stream
var stream_id = grpc_client.client_stream_start("/package.Service/Method", call_opts)

# Send messages incrementally
grpc_client.stream_send(stream_id, message_bytes_1)
grpc_client.stream_send(stream_id, message_bytes_2)
grpc_client.stream_send(stream_id, message_bytes_3)

# Close the send side (triggers server to send response)
grpc_client.stream_close_send(stream_id)

# Handle response via signal
grpc_client.message.connect(func(sid, data):
    if sid == stream_id:
        print("Received response: ", data)
)

grpc_client.finished.connect(func(sid, status_code, message):
    if sid == stream_id:
        print("Stream finished: ", status_code, message)
)
```

### Bidirectional Streaming RPC

Start a bidirectional stream where both client and server can send multiple messages concurrently.

```gdscript
# Start the stream
var stream_id = grpc_client.bidi_stream_start("/package.Service/Method", call_opts)

# Handle messages as they arrive
grpc_client.message.connect(func(sid, data):
    if sid == stream_id:
        print("Received message: ", data)
        # Can send more messages in response
        grpc_client.stream_send(stream_id, response_bytes)
)

# Send messages at any time
grpc_client.stream_send(stream_id, message_bytes_1)
await get_tree().create_timer(1.0).timeout
grpc_client.stream_send(stream_id, message_bytes_2)

# Optionally close send side when done writing
grpc_client.stream_close_send(stream_id)

# Stream will finish when server closes
grpc_client.finished.connect(func(sid, status_code, message):
    if sid == stream_id:
        print("Stream finished")
)
```

## New Methods

### `client_stream_start(method: String, call_opts: Dictionary = {}) -> int`

Start a client-streaming RPC call.

**Parameters:**
- `method`: Method name in format "/package.Service/Method"
- `call_opts`: Optional dictionary with:
  - `deadline_ms` (int): Deadline in milliseconds
  - `metadata` (Dictionary): Custom metadata key-value pairs

**Returns:** Stream ID (positive integer) on success, -1 on error

### `bidi_stream_start(method: String, call_opts: Dictionary = {}) -> int`

Start a bidirectional streaming RPC call.

**Parameters:** Same as `client_stream_start`

**Returns:** Stream ID (positive integer) on success, -1 on error

### `stream_send(stream_id: int, message_bytes: PackedByteArray) -> bool`

Send a message on an active stream (client-streaming or bidirectional only).

**Parameters:**
- `stream_id`: Stream ID returned from `client_stream_start` or `bidi_stream_start`
- `message_bytes`: Serialized protobuf message

**Returns:** `true` if queued successfully, `false` otherwise

### `stream_close_send(stream_id: int) -> void`

Close the send side of a stream (signal no more writes).

For client-streaming, this triggers the server to send its single response.
For bidirectional streaming, this indicates the client won't send more messages (server can still send).

**Parameters:**
- `stream_id`: Stream ID to close send on

### `stream_cancel(stream_id: int) -> void`

Cancel any active stream (server, client, or bidirectional).

This is a unified method that works with all stream types. `server_stream_cancel` remains for backward compatibility.

**Parameters:**
- `stream_id`: Stream ID to cancel

## Signals

All streaming types use the same signals:

### `message(stream_id: int, data: PackedByteArray)`

Emitted when a message is received from the server.

### `finished(stream_id: int, status_code: int, message: String)`

Emitted when the stream completes successfully.

### `error(stream_id: int, status_code: int, message: String)`

Emitted when the stream fails with an error.

## Implementation Details

### Architecture

- **GrpcStream** now supports three stream types via a `StreamType` enum
- **Writer thread**: Created for client-streaming and bidirectional streams to handle outgoing messages
- **Reader thread**: Created for all stream types to handle incoming messages
- **Write queue**: Thread-safe queue for buffering outgoing messages
- **Completion queue**: Each stream has its own gRPC completion queue for async operations

### Thread Safety

- All public methods are thread-safe using mutexes
- Signals are emitted on the main thread via `call_deferred()`
- Write operations are queued and processed by a dedicated writer thread

### Memory Management

- Streams are managed via `std::unique_ptr` and cleaned up automatically
- Active streams are tracked in a map and removed when finished/cancelled
- Threads are joined in the destructor to ensure clean shutdown

## Example: Echo Service

Here's a complete example using bidirectional streaming:

```gdscript
extends Node

var grpc_client = GrpcClient.new()
var stream_id = -1

func _ready():
    # Connect to server
    grpc_client.connect("dns:///localhost:50051", {})

    # Set up signal handlers
    grpc_client.message.connect(_on_message)
    grpc_client.finished.connect(_on_finished)
    grpc_client.error.connect(_on_error)

    # Start bidirectional stream
    stream_id = grpc_client.bidi_stream_start("/echo.Echo/BidiStream", {})

    # Send initial messages
    for i in range(5):
        var msg = PackedByteArray()
        # Serialize your protobuf message here
        grpc_client.stream_send(stream_id, msg)
        await get_tree().create_timer(0.5).timeout

func _on_message(sid: int, data: PackedByteArray):
    if sid == stream_id:
        print("Received echo: ", data.get_string_from_utf8())

func _on_finished(sid: int, status_code: int, message: String):
    if sid == stream_id:
        print("Stream finished successfully")

func _on_error(sid: int, status_code: int, message: String):
    if sid == stream_id:
        print("Stream error: ", message)

func _exit_tree():
    grpc_client.stream_close_send(stream_id)
    grpc_client.close()
```

## Known Limitations

1. **No flow control**: Messages are queued without backpressure
2. **No automatic reconnection**: Connection failures require manual handling
3. **No built-in Protobuf serialization**: Users must serialize messages externally (use godobuf or similar)

## Backward Compatibility

All existing APIs remain unchanged:
- `unary()` - Unary calls
- `server_stream_start()` - Server-streaming
- `server_stream_cancel()` - Cancel server stream (now aliased to `stream_cancel()`)

The new functionality is purely additive and doesn't break existing code.
