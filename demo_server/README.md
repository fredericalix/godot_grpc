# godot_grpc Demo Server

A simple gRPC server for testing the godot_grpc extension.

## Services

### 1. Greeter (helloworld.proto)
- **Method**: `SayHello` (unary)
- **Path**: `/helloworld.Greeter/SayHello`
- **Description**: Returns a greeting message

### 2. Monitor (metrics.proto)
- **Method**: `StreamMetrics` (server-streaming)
- **Path**: `/metrics.Monitor/StreamMetrics`
- **Description**: Streams random metric data points

## Prerequisites

- Go 1.21 or later
- Protocol Buffers compiler (`protoc`)
- Go gRPC plugins:
  ```bash
  go install google.golang.org/protobuf/cmd/protoc-gen-go@latest
  go install google.golang.org/grpc/cmd/protoc-gen-go-grpc@latest
  ```

## Building

1. Install dependencies:
   ```bash
   go mod download
   ```

2. Generate Go code from proto files:
   ```bash
   make proto
   ```

3. Build the server:
   ```bash
   make build
   ```

Or simply run:
```bash
make all
```

## Running

```bash
make run
```

Or directly:
```bash
./demo_server
```

The server will listen on `localhost:50051`.

## Testing with grpcurl

### Test SayHello (unary):
```bash
grpcurl -plaintext -d '{"name": "Godot"}' localhost:50051 helloworld.Greeter/SayHello
```

### Test StreamMetrics (server-streaming):
```bash
grpcurl -plaintext -d '{"interval_ms": 500, "count": 5}' localhost:50051 metrics.Monitor/StreamMetrics
```

## Cleaning

```bash
make clean
```
