# Code Examples

Practical code examples for common use cases with godot_grpc.

## Table of Contents

- [Basic Examples](#basic-examples)
  - [Simple Unary Call](#simple-unary-call)
  - [Server Streaming](#server-streaming)
  - [With Authentication](#with-authentication)
- [Game-Specific Examples](#game-specific-examples)
  - [Player Login System](#player-login-system)
  - [Leaderboard Updates](#leaderboard-updates)
  - [Real-time Chat](#real-time-chat)
  - [Matchmaking](#matchmaking)
- [Advanced Patterns](#advanced-patterns)
  - [Connection Manager Singleton](#connection-manager-singleton)
  - [Retry Logic](#retry-logic)
  - [Request Queue](#request-queue)
  - [Stream Multiplexer](#stream-multiplexer)
- [Protobuf Helpers](#protobuf-helpers)
  - [Manual Encoding](#manual-encoding)
  - [Manual Decoding](#manual-decoding)

---

## Basic Examples

### Simple Unary Call

Making a basic request-response call:

```gdscript
extends Node

var grpc_client: GrpcClient

func _ready():
    grpc_client = GrpcClient.new()

    if grpc_client.connect("dns:///localhost:50051"):
        get_user_profile("player123")

func get_user_profile(user_id: String):
    # Create request
    var request = PackedByteArray()
    request.append(0x0a)  # Field 1, wire type 2 (string)
    request.append(user_id.length())
    request.append_array(user_id.to_utf8_buffer())

    # Make call
    var response = grpc_client.unary("/users.UserService/GetProfile", request)

    if response.size() > 0:
        print("Got profile data: ", response)
    else:
        print("Failed to get profile")

func _exit_tree():
    grpc_client.close()
```

---

### Server Streaming

Receiving multiple messages from server:

```gdscript
extends Node

var grpc_client: GrpcClient
var stream_id: int = 0

func _ready():
    grpc_client = GrpcClient.new()

    # Connect signals
    grpc_client.message.connect(_on_message)
    grpc_client.finished.connect(_on_finished)
    grpc_client.error.connect(_on_error)

    # Connect and start stream
    if grpc_client.connect("dns:///localhost:50051"):
        start_news_feed()

func start_news_feed():
    var request = PackedByteArray()
    # Empty request or add filters

    stream_id = grpc_client.server_stream_start(
        "/news.NewsService/StreamUpdates",
        request
    )

    if stream_id > 0:
        print("News feed started")

func _on_message(sid: int, data: PackedByteArray):
    if sid == stream_id:
        # Parse news update
        var update = parse_news_update(data)
        display_news(update)

func _on_finished(sid: int, status: int):
    if sid == stream_id:
        print("News feed ended with status: ", status)

func _on_error(sid: int, code: int, msg: String):
    if sid == stream_id:
        print("News feed error: ", msg)

func parse_news_update(data: PackedByteArray) -> Dictionary:
    # Implement protobuf parsing
    return {}

func display_news(update: Dictionary):
    print("News: ", update)

func _exit_tree():
    if stream_id > 0:
        grpc_client.server_stream_cancel(stream_id)
    grpc_client.close()
```

---

### With Authentication

Adding authentication headers:

```gdscript
extends Node

var grpc_client: GrpcClient
var auth_token: String = ""

func _ready():
    grpc_client = GrpcClient.new()

    if grpc_client.connect("dns:///api.example.com:443", {"use_tls": 1}):
        login("alice", "password123")

func login(username: String, password: String):
    # Create login request
    var request = create_login_request(username, password)

    # No auth needed for login
    var response = grpc_client.unary("/auth.AuthService/Login", request)

    if response.size() > 0:
        auth_token = parse_token(response)
        print("Logged in! Token: ", auth_token)

        # Now make authenticated calls
        get_protected_data()

func get_protected_data():
    var request = PackedByteArray()

    # Add authentication metadata
    var call_opts = {
        "metadata": {
            "authorization": "Bearer " + auth_token
        }
    }

    var response = grpc_client.unary(
        "/api.DataService/GetProtectedData",
        request,
        call_opts
    )

    if response.size() > 0:
        print("Got protected data!")

func create_login_request(user: String, pass: String) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: username
    buf.append(0x0a)
    buf.append(user.length())
    buf.append_array(user.to_utf8_buffer())
    # Field 2: password
    buf.append(0x12)
    buf.append(pass.length())
    buf.append_array(pass.to_utf8_buffer())
    return buf

func parse_token(response: PackedByteArray) -> String:
    # Parse response and extract token
    if response.size() > 2:
        var len = response[1]
        return response.slice(2, 2 + len).get_string_from_utf8()
    return ""

func _exit_tree():
    grpc_client.close()
```

---

## Game-Specific Examples

### Player Login System

Complete login flow with session management:

```gdscript
class_name PlayerSession
extends Node

signal login_success(player_data: Dictionary)
signal login_failed(reason: String)

var grpc_client: GrpcClient
var session_token: String = ""
var player_id: String = ""

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.set_log_level(3)

func connect_to_server(endpoint: String) -> bool:
    var opts = {
        "use_tls": 1,
        "keepalive_time_ms": 30000,
        "max_receive_message_length": 4194304
    }
    return grpc_client.connect(endpoint, opts)

func login(username: String, password: String):
    var request = _encode_login_request(username, password)

    var call_opts = {
        "timeout_ms": 10000  # 10 second timeout
    }

    var response = grpc_client.unary(
        "/game.AuthService/Login",
        request,
        call_opts
    )

    if response.size() == 0:
        login_failed.emit("Connection failed or timeout")
        return

    var result = _decode_login_response(response)

    if result.success:
        session_token = result.token
        player_id = result.player_id
        login_success.emit(result.player_data)
    else:
        login_failed.emit(result.error_message)

func make_authenticated_call(method: String, request: PackedByteArray) -> PackedByteArray:
    if session_token.is_empty():
        push_error("Not authenticated")
        return PackedByteArray()

    var call_opts = {
        "metadata": {
            "authorization": "Bearer " + session_token,
            "x-player-id": player_id
        },
        "timeout_ms": 5000
    }

    return grpc_client.unary(method, request, call_opts)

func _encode_login_request(user: String, pass: String) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: username (string)
    buf.append(0x0a)
    buf.append(user.length())
    buf.append_array(user.to_utf8_buffer())
    # Field 2: password (string)
    buf.append(0x12)
    buf.append(pass.length())
    buf.append_array(pass.to_utf8_buffer())
    return buf

func _decode_login_response(data: PackedByteArray) -> Dictionary:
    # Simple parser - in production use proper protobuf library
    var result = {
        "success": false,
        "token": "",
        "player_id": "",
        "player_data": {},
        "error_message": ""
    }

    # Parse fields (simplified)
    var i = 0
    while i < data.size():
        if i >= data.size():
            break
        var tag = data[i]
        i += 1

        var field_num = tag >> 3
        match field_num:
            1:  # success (bool)
                result.success = data[i] != 0
                i += 1
            2:  # token (string)
                var len = data[i]
                i += 1
                result.token = data.slice(i, i + len).get_string_from_utf8()
                i += len
            3:  # player_id (string)
                var len = data[i]
                i += 1
                result.player_id = data.slice(i, i + len).get_string_from_utf8()
                i += len

    return result

func _exit_tree():
    grpc_client.close()
```

**Usage:**

```gdscript
extends Control

var session: PlayerSession

func _ready():
    session = PlayerSession.new()
    add_child(session)

    session.login_success.connect(_on_login_success)
    session.login_failed.connect(_on_login_failed)

    if session.connect_to_server("dns:///game.example.com:443"):
        session.login("player1", "secret")

func _on_login_success(player_data: Dictionary):
    print("Logged in! Player data: ", player_data)
    # Switch to game scene
    get_tree().change_scene_to_file("res://scenes/game.tscn")

func _on_login_failed(reason: String):
    print("Login failed: ", reason)
    $ErrorLabel.text = "Login failed: " + reason
```

---

### Leaderboard Updates

Streaming leaderboard updates:

```gdscript
class_name Leaderboard
extends Control

var grpc_client: GrpcClient
var stream_id: int = 0

@onready var list_container = $VBoxContainer/ScrollContainer/ListContainer

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_leaderboard_update)
    grpc_client.finished.connect(_on_stream_finished)

    if grpc_client.connect("dns:///game-server.com:443", {"use_tls": 1}):
        start_leaderboard_stream()

func start_leaderboard_stream():
    # Request top 100 players
    var request = _encode_leaderboard_request(100)

    stream_id = grpc_client.server_stream_start(
        "/game.LeaderboardService/StreamLeaderboard",
        request
    )

    if stream_id > 0:
        print("Leaderboard stream started")

func _on_leaderboard_update(sid: int, data: PackedByteArray):
    if sid != stream_id:
        return

    var entry = _parse_leaderboard_entry(data)

    # Update UI
    var label = Label.new()
    label.text = "%d. %s - %d points" % [entry.rank, entry.player_name, entry.score]
    list_container.add_child(label)

func _encode_leaderboard_request(limit: int) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: limit (int32)
    buf.append(0x08)
    buf.append_array(_encode_varint(limit))
    return buf

func _parse_leaderboard_entry(data: PackedByteArray) -> Dictionary:
    return {
        "rank": 0,
        "player_name": "",
        "player_id": "",
        "score": 0
    }
    # Implement proper parsing

func _encode_varint(value: int) -> PackedByteArray:
    var buf = PackedByteArray()
    while value > 127:
        buf.append((value & 0x7F) | 0x80)
        value >>= 7
    buf.append(value & 0x7F)
    return buf

func _on_stream_finished(sid: int, status: int):
    if sid == stream_id:
        print("Leaderboard stream complete")

func _exit_tree():
    if stream_id > 0:
        grpc_client.server_stream_cancel(stream_id)
    grpc_client.close()
```

---

### Real-time Chat

Implementing a chat system with server streaming:

```gdscript
class_name ChatSystem
extends Control

signal message_received(sender: String, text: String)

var grpc_client: GrpcClient
var stream_id: int = 0
var room_id: String = ""

@onready var message_list = $VBoxContainer/MessageList
@onready var input_field = $VBoxContainer/HBoxContainer/LineEdit
@onready var send_button = $VBoxContainer/HBoxContainer/SendButton

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_chat_message)

    send_button.pressed.connect(_on_send_clicked)

func join_room(room: String, auth_token: String):
    room_id = room

    if !grpc_client.connect("dns:///chat.example.com:443", {"use_tls": 1}):
        print("Failed to connect")
        return

    var request = _encode_join_request(room_id)

    var call_opts = {
        "metadata": {
            "authorization": "Bearer " + auth_token
        }
    }

    stream_id = grpc_client.server_stream_start(
        "/chat.ChatService/JoinRoom",
        request,
        call_opts
    )

    if stream_id > 0:
        print("Joined room: ", room_id)

func send_message(text: String):
    if text.is_empty():
        return

    var request = _encode_chat_message(room_id, text)

    var response = grpc_client.unary("/chat.ChatService/SendMessage", request)

    if response.size() > 0:
        input_field.text = ""

func _on_chat_message(sid: int, data: PackedByteArray):
    if sid != stream_id:
        return

    var msg = _parse_chat_message(data)

    # Update UI
    var label = Label.new()
    label.text = "[%s] %s" % [msg.sender, msg.text]
    message_list.add_child(label)

    message_received.emit(msg.sender, msg.text)

func _on_send_clicked():
    send_message(input_field.text)

func _encode_join_request(room: String) -> PackedByteArray:
    var buf = PackedByteArray()
    buf.append(0x0a)
    buf.append(room.length())
    buf.append_array(room.to_utf8_buffer())
    return buf

func _encode_chat_message(room: String, text: String) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: room_id
    buf.append(0x0a)
    buf.append(room.length())
    buf.append_array(room.to_utf8_buffer())
    # Field 2: text
    buf.append(0x12)
    buf.append(text.length())
    buf.append_array(text.to_utf8_buffer())
    return buf

func _parse_chat_message(data: PackedByteArray) -> Dictionary:
    # Implement proper parsing
    return {
        "sender": "",
        "text": "",
        "timestamp": 0
    }

func _exit_tree():
    if stream_id > 0:
        grpc_client.server_stream_cancel(stream_id)
    grpc_client.close()
```

---

### Matchmaking

Server-streaming matchmaking with status updates:

```gdscript
class_name Matchmaker
extends Node

signal matchmaking_started()
signal player_found(player_count: int, total_needed: int)
signal match_found(match_data: Dictionary)
signal matchmaking_cancelled()

var grpc_client: GrpcClient
var stream_id: int = 0
var is_searching: bool = false

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_matchmaking_update)
    grpc_client.finished.connect(_on_matchmaking_finished)

func start_matchmaking(player_id: String, game_mode: String):
    if is_searching:
        print("Already searching")
        return

    if !grpc_client.is_connected():
        if !grpc_client.connect("dns:///matchmaking.example.com:443", {"use_tls": 1}):
            print("Failed to connect")
            return

    var request = _encode_matchmaking_request(player_id, game_mode)

    stream_id = grpc_client.server_stream_start(
        "/matchmaking.MatchmakingService/FindMatch",
        request
    )

    if stream_id > 0:
        is_searching = true
        matchmaking_started.emit()
        print("Searching for match...")

func cancel_matchmaking():
    if stream_id > 0:
        grpc_client.server_stream_cancel(stream_id)
        stream_id = 0
        is_searching = false
        matchmaking_cancelled.emit()

func _on_matchmaking_update(sid: int, data: PackedByteArray):
    if sid != stream_id:
        return

    var update = _parse_matchmaking_update(data)

    match update.type:
        "SEARCHING":
            print("Searching for players...")

        "PLAYER_FOUND":
            print("Found %d/%d players" % [update.current_count, update.needed_count])
            player_found.emit(update.current_count, update.needed_count)

        "MATCH_READY":
            print("Match ready!")
            is_searching = false
            match_found.emit(update.match_data)

func _on_matchmaking_finished(sid: int, status: int):
    if sid == stream_id:
        is_searching = false
        if status == 0:
            print("Matchmaking completed")
        else:
            print("Matchmaking failed with status: ", status)

func _encode_matchmaking_request(player: String, mode: String) -> PackedByteArray:
    var buf = PackedByteArray()
    # Field 1: player_id
    buf.append(0x0a)
    buf.append(player.length())
    buf.append_array(player.to_utf8_buffer())
    # Field 2: game_mode
    buf.append(0x12)
    buf.append(mode.length())
    buf.append_array(mode.to_utf8_buffer())
    return buf

func _parse_matchmaking_update(data: PackedByteArray) -> Dictionary:
    # Implement proper parsing
    return {
        "type": "SEARCHING",
        "current_count": 0,
        "needed_count": 0,
        "match_data": {}
    }

func _exit_tree():
    cancel_matchmaking()
    grpc_client.close()
```

---

## Advanced Patterns

### Connection Manager Singleton

Manage a single connection across your entire game:

```gdscript
# autoload/grpc_manager.gd
extends Node

var client: GrpcClient
var is_connected: bool = false

@export var server_endpoint: String = "dns:///api.example.com:443"
@export var use_tls: bool = true

func _ready():
    client = GrpcClient.new()
    client.set_log_level(2)  # WARN level

func connect_to_server() -> bool:
    if is_connected:
        return true

    var opts = {
        "use_tls": 1 if use_tls else 0,
        "keepalive_time_ms": 30000,
        "keepalive_timeout_ms": 10000,
        "max_receive_message_length": 8388608  # 8MB
    }

    is_connected = client.connect(server_endpoint, opts)

    if is_connected:
        print("Connected to game server")
    else:
        push_error("Failed to connect to game server")

    return is_connected

func disconnect_from_server():
    if client:
        client.close()
        is_connected = false

func make_call(method: String, request: PackedByteArray, auth_token: String = "") -> PackedByteArray:
    if !is_connected:
        push_error("Not connected to server")
        return PackedByteArray()

    var call_opts = {}

    if !auth_token.is_empty():
        call_opts["metadata"] = {
            "authorization": "Bearer " + auth_token
        }

    return client.unary(method, request, call_opts)

func _exit_tree():
    disconnect_from_server()
```

**Usage:**

```gdscript
# In any scene
func _ready():
    if GrpcManager.connect_to_server():
        var response = GrpcManager.make_call("/service/Method", request, token)
```

---

### Retry Logic

Automatic retry with exponential backoff:

```gdscript
class_name RetryableCall
extends RefCounted

var grpc_client: GrpcClient
var max_retries: int = 3
var base_delay_ms: int = 1000

func call_with_retry(
    method: String,
    request: PackedByteArray,
    call_opts: Dictionary = {}
) -> PackedByteArray:

    var attempt = 0

    while attempt < max_retries:
        var response = grpc_client.unary(method, request, call_opts)

        if response.size() > 0:
            return response

        # Failed - retry with exponential backoff
        attempt += 1
        if attempt < max_retries:
            var delay = base_delay_ms * pow(2, attempt - 1)
            print("Retry %d/%d after %dms" % [attempt, max_retries, delay])
            await Engine.get_main_loop().create_timer(delay / 1000.0).timeout

    push_error("Call failed after %d retries" % max_retries)
    return PackedByteArray()
```

---

### Request Queue

Queue requests when offline and send when reconnected:

```gdscript
class_name RequestQueue
extends Node

var grpc_client: GrpcClient
var queue: Array[Dictionary] = []
var is_processing: bool = false

func enqueue(method: String, request: PackedByteArray, callback: Callable):
    queue.append({
        "method": method,
        "request": request,
        "callback": callback
    })

    if !is_processing:
        process_queue()

func process_queue():
    if queue.is_empty():
        is_processing = false
        return

    is_processing = true
    var item = queue.pop_front()

    var response = grpc_client.unary(item.method, item.request)

    if response.size() > 0:
        item.callback.call(response)
    else:
        # Re-queue on failure
        queue.push_front(item)
        await get_tree().create_timer(5.0).timeout

    process_queue()
```

---

### Stream Multiplexer

Manage multiple streams:

```gdscript
class_name StreamMultiplexer
extends Node

var grpc_client: GrpcClient
var streams: Dictionary = {}  # stream_id -> handler

func _ready():
    grpc_client = GrpcClient.new()
    grpc_client.message.connect(_on_message)
    grpc_client.finished.connect(_on_finished)

func create_stream(
    name: String,
    method: String,
    request: PackedByteArray,
    handler: Callable
) -> bool:

    var stream_id = grpc_client.server_stream_start(method, request)

    if stream_id > 0:
        streams[stream_id] = {
            "name": name,
            "handler": handler
        }
        return true

    return false

func cancel_stream(name: String):
    for stream_id in streams:
        if streams[stream_id].name == name:
            grpc_client.server_stream_cancel(stream_id)
            streams.erase(stream_id)
            return

func _on_message(stream_id: int, data: PackedByteArray):
    if stream_id in streams:
        streams[stream_id].handler.call(data)

func _on_finished(stream_id: int, status: int):
    if stream_id in streams:
        print("Stream '%s' finished" % streams[stream_id].name)
        streams.erase(stream_id)

func _exit_tree():
    for stream_id in streams:
        grpc_client.server_stream_cancel(stream_id)
    grpc_client.close()
```

---

## Protobuf Helpers

### Manual Encoding

Helper functions for encoding common protobuf types:

```gdscript
class_name ProtobufEncoder

static func encode_string(field_num: int, value: String) -> PackedByteArray:
    var buf = PackedByteArray()
    buf.append((field_num << 3) | 2)  # Wire type 2
    buf.append(value.length())
    buf.append_array(value.to_utf8_buffer())
    return buf

static func encode_int32(field_num: int, value: int) -> PackedByteArray:
    var buf = PackedByteArray()
    buf.append((field_num << 3) | 0)  # Wire type 0
    buf.append_array(encode_varint(value))
    return buf

static func encode_bool(field_num: int, value: bool) -> PackedByteArray:
    var buf = PackedByteArray()
    buf.append((field_num << 3) | 0)
    buf.append(1 if value else 0)
    return buf

static func encode_double(field_num: int, value: float) -> PackedByteArray:
    var buf = PackedByteArray()
    buf.append((field_num << 3) | 1)  # Wire type 1
    buf.resize(9)
    buf.encode_double(1, value)
    return buf

static func encode_varint(value: int) -> PackedByteArray:
    var buf = PackedByteArray()
    while value > 127:
        buf.append((value & 0x7F) | 0x80)
        value >>= 7
    buf.append(value & 0x7F)
    return buf
```

**Usage:**

```gdscript
var request = PackedByteArray()
request.append_array(ProtobufEncoder.encode_string(1, "alice"))
request.append_array(ProtobufEncoder.encode_int32(2, 42))
request.append_array(ProtobufEncoder.encode_bool(3, true))
```

---

### Manual Decoding

Helper functions for decoding protobuf messages:

```gdscript
class_name ProtobufDecoder

static func decode_message(data: PackedByteArray) -> Dictionary:
    var result = {}
    var i = 0

    while i < data.size():
        var tag_result = read_varint(data, i)
        var tag = tag_result.value
        i = tag_result.index

        var field_num = tag >> 3
        var wire_type = tag & 0x07

        match wire_type:
            0:  # Varint
                var val_result = read_varint(data, i)
                result[field_num] = val_result.value
                i = val_result.index

            1:  # 64-bit
                result[field_num] = data.decode_double(i)
                i += 8

            2:  # Length-delimited
                var len = data[i]
                i += 1
                result[field_num] = data.slice(i, i + len)
                i += len

            5:  # 32-bit
                result[field_num] = data.decode_float(i)
                i += 4

    return result

static func read_varint(data: PackedByteArray, start: int) -> Dictionary:
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

    return {"value": value, "index": i}

static func get_string(field_data) -> String:
    if field_data is PackedByteArray:
        return field_data.get_string_from_utf8()
    return ""
```

**Usage:**

```gdscript
var fields = ProtobufDecoder.decode_message(response_data)
var name = ProtobufDecoder.get_string(fields.get(1, PackedByteArray()))
var age = fields.get(2, 0)
var active = fields.get(3, 0) != 0
```

---

## Additional Resources

- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Tutorial](TUTORIAL.md) - Step-by-step guide
- [Developer Guide](DEVELOPER_GUIDE.md) - Contributing to godot_grpc

For more examples, check the `demo/` directory in the repository.
