#ifndef GODOT_GRPC_STREAM_H
#define GODOT_GRPC_STREAM_H

#include <grpcpp/grpcpp.h>
#include <grpcpp/generic/generic_stub.h>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <functional>

namespace godot_grpc {

/**
 * Callback types for stream events.
 * These callbacks are invoked from background threads and must be
 * thread-safe. The recipient should dispatch to the main thread.
 */
using StreamMessageCallback = std::function<void(int stream_id, const godot::PackedByteArray& data)>;
using StreamFinishedCallback = std::function<void(int stream_id, int status_code, const std::string& message)>;
using StreamErrorCallback = std::function<void(int stream_id, int status_code, const std::string& message)>;

/**
 * Manages a single server-streaming RPC call.
 * Runs a reader thread that pulls messages from the gRPC stream
 * and invokes callbacks when messages arrive or the stream finishes.
 */
class GrpcStream {
public:
    GrpcStream(
        int stream_id,
        std::shared_ptr<grpc::GenericStub> stub,
        const std::string& method,
        const godot::PackedByteArray& request_bytes,
        std::unique_ptr<grpc::ClientContext> context,
        StreamMessageCallback on_message,
        StreamFinishedCallback on_finished,
        StreamErrorCallback on_error
    );

    ~GrpcStream();

    // Start the stream (spawns the reader thread).
    void start();

    // Cancel the stream gracefully.
    void cancel();

    // Get the stream ID.
    int get_id() const { return stream_id_; }

    // Check if the stream is still active.
    bool is_active() const { return active_.load(); }

private:
    void reader_thread();

    int stream_id_;
    std::shared_ptr<grpc::GenericStub> stub_;
    std::string method_;
    godot::PackedByteArray request_bytes_;
    std::unique_ptr<grpc::ClientContext> context_;

    StreamMessageCallback on_message_;
    StreamFinishedCallback on_finished_;
    StreamErrorCallback on_error_;

    std::atomic<bool> active_;
    std::unique_ptr<std::thread> reader_thread_;
};

} // namespace godot_grpc

#endif // GODOT_GRPC_STREAM_H
