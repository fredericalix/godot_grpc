extends Node

## godot_grpc Demo Script
##
## This script demonstrates how to use the GrpcClient extension to:
## - Connect to a gRPC server
## - Make unary RPC calls
## - Handle server-streaming RPCs
## - Process messages, errors, and completion events
##
## Prerequisites:
## - Build the godot_grpc extension
## - Run the demo server: cd demo_server && make run

var grpc_client: GrpcClient
var active_streams: Array[int] = []

func _ready() -> void:
	print("=== godot_grpc Demo ===")
	print("This demo requires the demo server to be running.")
	print("Start it with: cd demo_server && make run")
	print("")

	# Create the gRPC client
	grpc_client = GrpcClient.new()

	# Set log level (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE)
	grpc_client.set_log_level(3)  # INFO level

	# Connect signals for streaming
	grpc_client.message.connect(_on_stream_message)
	grpc_client.finished.connect(_on_stream_finished)
	grpc_client.error.connect(_on_stream_error)

	# Connect to the server
	var endpoint := "dns:///localhost:50051"
	var options := {
		"keepalive_seconds": 30,
		"enable_tls": false
	}

	print("Connecting to %s..." % endpoint)
	var connected := grpc_client.connect(endpoint, options)

	if not connected:
		print("ERROR: Failed to connect to server!")
		return

	print("Connected successfully!")
	print("")

	# Wait a moment for connection to stabilize
	await get_tree().create_timer(0.5).timeout

	# Demo 1: Unary call
	await demo_unary_call()

	# Demo 2: Server streaming
	await demo_server_streaming()

## Demo 1: Unary RPC call to /helloworld.Greeter/SayHello
func demo_unary_call() -> void:
	print("--- Demo 1: Unary Call ---")

	# In a real application, you would use a proper Protobuf library
	# like godobuf (https://github.com/oniksan/godobuf) to serialize messages.
	# For this demo, we'll create a simple manual serialization.

	var request_bytes := _create_hello_request("Godot Player")

	var call_opts := {
		"deadline_ms": 5000,  # 5 second deadline
		"metadata": {
			"client-type": "godot",
			"demo-version": "1.0"
		}
	}

	print("Calling /helloworld.Greeter/SayHello...")
	var response_bytes := grpc_client.unary(
		"/helloworld.Greeter/SayHello",
		request_bytes,
		call_opts
	)

	if response_bytes.size() > 0:
		var message := _parse_hello_reply(response_bytes)
		print("SUCCESS: Received response: %s" % message)
	else:
		print("ERROR: Empty response or call failed")

	print("")
	await get_tree().create_timer(1.0).timeout

## Demo 2: Server-streaming RPC to /metrics.Monitor/StreamMetrics
func demo_server_streaming() -> void:
	print("--- Demo 2: Server Streaming ---")

	var request_bytes := _create_metrics_request(500, 5)  # 500ms interval, 5 metrics

	var call_opts := {
		"deadline_ms": 10000,  # 10 second deadline
	}

	print("Starting server stream /metrics.Monitor/StreamMetrics...")
	var stream_id := grpc_client.server_stream_start(
		"/metrics.Monitor/StreamMetrics",
		request_bytes,
		call_opts
	)

	if stream_id < 0:
		print("ERROR: Failed to start stream")
		return

	print("Stream started with ID: %d" % stream_id)
	active_streams.append(stream_id)

	# The stream will emit signals as messages arrive
	# Wait for stream to complete
	await get_tree().create_timer(5.0).timeout

	print("")

## Signal handler: Called when a stream message is received
func _on_stream_message(stream_id: int, data: PackedByteArray) -> void:
	var metric := _parse_metric_data(data)
	print("  [Stream %d] Metric received: %s = %.2f (timestamp: %d)" % [
		stream_id,
		metric.name,
		metric.value,
		metric.timestamp
	])

## Signal handler: Called when a stream finishes successfully
func _on_stream_finished(stream_id: int, status_code: int, message: String) -> void:
	print("  [Stream %d] FINISHED - Status: %d, Message: %s" % [stream_id, status_code, message])
	active_streams.erase(stream_id)

## Signal handler: Called when a stream encounters an error
func _on_stream_error(stream_id: int, status_code: int, message: String) -> void:
	print("  [Stream %d] ERROR - Status: %d, Message: %s" % [stream_id, status_code, message])
	active_streams.erase(stream_id)

## Cleanup when exiting
func _exit_tree() -> void:
	# Cancel all active streams
	for stream_id in active_streams:
		grpc_client.server_stream_cancel(stream_id)

	# Close the connection
	if grpc_client:
		grpc_client.close()
		print("gRPC client closed")

# ============================================================================
# PROTOBUF HELPERS (Simplified for demo purposes)
# In a real application, use a proper Protobuf library like godobuf
# ============================================================================

## Create a HelloRequest protobuf message
## Message format: field 1 (string name)
func _create_hello_request(name: String) -> PackedByteArray:
	var buffer := PackedByteArray()
	# Field 1: string name
	# Tag: (field_number << 3) | wire_type = (1 << 3) | 2 = 0x0a
	buffer.append(0x0a)
	buffer.append(name.length())
	buffer.append_array(name.to_utf8_buffer())
	return buffer

## Parse a HelloReply protobuf message
## Message format: field 1 (string message)
func _parse_hello_reply(data: PackedByteArray) -> String:
	if data.size() < 2:
		return ""
	# Skip tag byte
	var length := data[1]
	if data.size() < 2 + length:
		return ""
	var message_bytes := data.slice(2, 2 + length)
	return message_bytes.get_string_from_utf8()

## Create a MetricsRequest protobuf message
## Message format: field 1 (int32 interval_ms), field 2 (int32 count)
func _create_metrics_request(interval_ms: int, count: int) -> PackedByteArray:
	var buffer := PackedByteArray()

	# Field 1: int32 interval_ms
	# Tag: (1 << 3) | 0 = 0x08
	buffer.append(0x08)
	buffer.append_array(_encode_varint(interval_ms))

	# Field 2: int32 count
	# Tag: (2 << 3) | 0 = 0x10
	buffer.append(0x10)
	buffer.append_array(_encode_varint(count))

	return buffer

## Parse a MetricData protobuf message
## Message format: field 1 (string name), field 2 (double value), field 3 (int64 timestamp)
func _parse_metric_data(data: PackedByteArray) -> Dictionary:
	var result := {
		"name": "",
		"value": 0.0,
		"timestamp": 0
	}

	var pos := 0
	while pos < data.size():
		if pos + 1 >= data.size():
			break

		var tag := data[pos]
		var field_number := tag >> 3
		var wire_type := tag & 0x07
		pos += 1

		if field_number == 1 and wire_type == 2:  # string name
			var length := data[pos]
			pos += 1
			if pos + length <= data.size():
				result.name = data.slice(pos, pos + length).get_string_from_utf8()
				pos += length
		elif field_number == 2 and wire_type == 1:  # double value
			if pos + 8 <= data.size():
				result.value = data.decode_double(pos)
				pos += 8
		elif field_number == 3 and wire_type == 0:  # int64 timestamp
			var value_result := _decode_varint(data, pos)
			result.timestamp = value_result.value
			pos = value_result.next_pos
		else:
			# Unknown field, skip
			break

	return result

## Encode an integer as a protobuf varint
func _encode_varint(value: int) -> PackedByteArray:
	var buffer := PackedByteArray()
	while value > 127:
		buffer.append((value & 0x7F) | 0x80)
		value >>= 7
	buffer.append(value & 0x7F)
	return buffer

## Decode a protobuf varint
func _decode_varint(data: PackedByteArray, start_pos: int) -> Dictionary:
	var value := 0
	var shift := 0
	var pos := start_pos

	while pos < data.size():
		var byte := data[pos]
		value |= (byte & 0x7F) << shift
		pos += 1
		if (byte & 0x80) == 0:
			break
		shift += 7

	return {"value": value, "next_pos": pos}
