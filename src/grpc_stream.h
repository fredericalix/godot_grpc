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
#include <queue>
#include <condition_variable>

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
 * Stream type enum for different gRPC streaming patterns.
 */
enum class StreamType {
    SERVER_STREAMING,  // Client sends one, server sends many
    CLIENT_STREAMING,  // Client sends many, server sends one
    BIDIRECTIONAL      // Both send many
};

/**
 * Manages a single streaming RPC call (server, client, or bidirectional).
 * Runs reader/writer threads that handle messages from/to the gRPC stream
 * and invokes callbacks when messages arrive or the stream finishes.
 */
class GrpcStream {
public:
    GrpcStream(
        int stream_id,
        StreamType stream_type,
        std::shared_ptr<grpc::GenericStub> stub,
        const std::string& method,
        const godot::PackedByteArray& request_bytes,
        std::unique_ptr<grpc::ClientContext> context,
        StreamMessageCallback on_message,
        StreamFinishedCallback on_finished,
        StreamErrorCallback on_error
    );

    ~GrpcStream();

    // Start the stream (spawns the reader/writer threads).
    void start();

    // Cancel the stream gracefully.
    void cancel();

    // Send a message on the stream (for client-streaming and bidirectional).
    // Returns true if queued successfully, false if stream is closed.
    bool send(const godot::PackedByteArray& message_bytes);

    // Close the send side of the stream (calls WritesDone).
    void close_send();

    // Get the stream ID.
    int get_id() const { return stream_id_; }

    // Check if the stream is still active.
    bool is_active() const { return active_.load(); }

private:
    void reader_thread();
    void writer_thread();

    int stream_id_;
    StreamType stream_type_;
    std::shared_ptr<grpc::GenericStub> stub_;
    std::string method_;
    godot::PackedByteArray initial_request_bytes_;
    std::unique_ptr<grpc::ClientContext> context_;

    StreamMessageCallback on_message_;
    StreamFinishedCallback on_finished_;
    StreamErrorCallback on_error_;

    std::atomic<bool> active_;
    std::atomic<bool> writes_done_;
    std::unique_ptr<std::thread> reader_thread_;
    std::unique_ptr<std::thread> writer_thread_;

    // Write queue for client-streaming and bidirectional
    std::mutex write_queue_mutex_;
    std::condition_variable write_queue_cv_;
    std::queue<godot::PackedByteArray> write_queue_;
    bool write_queue_closed_;

    // Shared stream object
    std::shared_ptr<grpc::GenericClientAsyncReaderWriter> stream_;
    std::shared_ptr<grpc::CompletionQueue> cq_;
};

} // namespace godot_grpc

#endif // GODOT_GRPC_STREAM_H
