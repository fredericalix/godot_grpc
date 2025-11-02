#ifndef GODOT_GRPC_CLIENT_H
#define GODOT_GRPC_CLIENT_H

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include "grpc_channel_pool.h"
#include "grpc_stream.h"
#include <memory>
#include <map>
#include <mutex>

namespace godot_grpc {

/**
 * GrpcClient: The main class exposed to Godot for gRPC client functionality.
 *
 * Provides:
 * - Channel lifecycle management (connect, close)
 * - Unary RPC calls
 * - Server-streaming RPC calls
 * - Signals for streaming events
 */
class GrpcClient : public godot::RefCounted {
    GDCLASS(GrpcClient, godot::RefCounted)

public:
    GrpcClient();
    ~GrpcClient();

    // Lifecycle
    /**
     * Connect to a gRPC server.
     *
     * @param endpoint Server address (e.g., "dns:///localhost:50051", "ipv4:127.0.0.1:50051")
     * @param options Dictionary with optional keys:
     *   - max_retries (int): Maximum number of retry attempts
     *   - keepalive_seconds (int): Keepalive interval in seconds
     *   - enable_tls (bool): Enable TLS encryption
     *   - authority (String): Custom authority header
     *   - max_send_message_length (int): Max send message size in bytes
     *   - max_receive_message_length (int): Max receive message size in bytes
     * @return true if connection was successful
     */
    bool connect(const godot::String& endpoint, const godot::Dictionary& options = godot::Dictionary());

    /**
     * Close the connection and cancel all in-flight calls.
     */
    void close();

    /**
     * Check if the client is connected.
     */
    bool is_connected() const;

    // Unary RPC
    /**
     * Make a unary RPC call.
     *
     * @param full_method Method name in format "/package.Service/Method"
     * @param request_bytes Serialized request message
     * @param call_opts Dictionary with optional keys:
     *   - deadline_ms (int): Deadline in milliseconds from now
     *   - metadata (Dictionary): Custom metadata key-value pairs
     * @return Serialized response message bytes, or empty array on error
     */
    godot::PackedByteArray unary(
        const godot::String& full_method,
        const godot::PackedByteArray& request_bytes,
        const godot::Dictionary& call_opts = godot::Dictionary()
    );

    // Server-streaming RPC
    /**
     * Start a server-streaming RPC call.
     *
     * @param full_method Method name in format "/package.Service/Method"
     * @param request_bytes Serialized request message
     * @param call_opts Dictionary with optional keys:
     *   - deadline_ms (int): Deadline in milliseconds from now
     *   - metadata (Dictionary): Custom metadata key-value pairs
     * @return Stream ID (positive integer) on success, -1 on error
     */
    int server_stream_start(
        const godot::String& full_method,
        const godot::PackedByteArray& request_bytes,
        const godot::Dictionary& call_opts = godot::Dictionary()
    );

    /**
     * Cancel a server-streaming RPC call.
     *
     * @param stream_id Stream ID returned from server_stream_start
     */
    void server_stream_cancel(int stream_id);

    // Client-streaming RPC
    /**
     * Start a client-streaming RPC call.
     *
     * @param full_method Method name in format "/package.Service/Method"
     * @param call_opts Dictionary with optional keys:
     *   - deadline_ms (int): Deadline in milliseconds from now
     *   - metadata (Dictionary): Custom metadata key-value pairs
     * @return Stream ID (positive integer) on success, -1 on error
     */
    int client_stream_start(
        const godot::String& full_method,
        const godot::Dictionary& call_opts = godot::Dictionary()
    );

    // Bidirectional streaming RPC
    /**
     * Start a bidirectional streaming RPC call.
     *
     * @param full_method Method name in format "/package.Service/Method"
     * @param call_opts Dictionary with optional keys:
     *   - deadline_ms (int): Deadline in milliseconds from now
     *   - metadata (Dictionary): Custom metadata key-value pairs
     * @return Stream ID (positive integer) on success, -1 on error
     */
    int bidi_stream_start(
        const godot::String& full_method,
        const godot::Dictionary& call_opts = godot::Dictionary()
    );

    // Stream management (for client and bidirectional streams)
    /**
     * Send a message on an active stream (client or bidirectional streaming only).
     *
     * @param stream_id Stream ID returned from client_stream_start or bidi_stream_start
     * @param message_bytes Serialized message to send
     * @return true if message was queued successfully, false otherwise
     */
    bool stream_send(int stream_id, const godot::PackedByteArray& message_bytes);

    /**
     * Close the send side of a stream (signal no more writes).
     * For client-streaming, this triggers the server to send its response.
     *
     * @param stream_id Stream ID to close send on
     */
    void stream_close_send(int stream_id);

    /**
     * Cancel any active stream.
     *
     * @param stream_id Stream ID to cancel
     */
    void stream_cancel(int stream_id);

    // Logging
    /**
     * Set the log level for the extension.
     *
     * @param level Log level (0=NONE, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE)
     */
    void set_log_level(int level);

    /**
     * Get the current log level.
     */
    int get_log_level() const;

protected:
    static void _bind_methods();

private:
    // Helper to parse call options
    std::unique_ptr<grpc::ClientContext> create_context(const godot::Dictionary& call_opts);

    // Helper to parse channel options
    ChannelOptions parse_channel_options(const godot::Dictionary& options);

    // Helper to start a stream of a specific type
    int start_stream(
        StreamType stream_type,
        const godot::String& full_method,
        const godot::PackedByteArray& request_bytes,
        const godot::Dictionary& call_opts
    );

    // Stream callbacks (called from background threads)
    void on_stream_message(int stream_id, const godot::PackedByteArray& data);
    void on_stream_finished(int stream_id, int status_code, const std::string& message);
    void on_stream_error(int stream_id, int status_code, const std::string& message);

    // Channel management
    GrpcChannelPool channel_pool_;

    // Active streams
    std::mutex streams_mutex_;
    std::map<int, std::unique_ptr<GrpcStream>> active_streams_;
    int next_stream_id_;

    // Deferred signal emission (to be called on main thread)
    struct PendingSignal {
        enum Type { MESSAGE, FINISHED, ERROR };
        Type type;
        int stream_id;
        godot::PackedByteArray data;
        int status_code;
        godot::String message;
    };

    std::mutex pending_signals_mutex_;
    std::vector<PendingSignal> pending_signals_;

    void process_pending_signals();
};

} // namespace godot_grpc

#endif // GODOT_GRPC_CLIENT_H
