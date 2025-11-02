#include "grpc_client.h"
#include "util/status_map.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <grpcpp/support/byte_buffer.h>
#include <grpcpp/support/slice.h>
#include <grpcpp/support/sync_stream.h>
#include <chrono>

namespace godot_grpc {

GrpcClient::GrpcClient()
    : next_stream_id_(1)
{
    Logger::debug("GrpcClient created");
}

GrpcClient::~GrpcClient() {
    Logger::debug("GrpcClient destroyed");
    close();
}

void GrpcClient::_bind_methods() {
    // Lifecycle methods
    godot::ClassDB::bind_method(godot::D_METHOD("connect", "endpoint", "options"), &GrpcClient::connect, DEFVAL(godot::Dictionary()));
    godot::ClassDB::bind_method(godot::D_METHOD("close"), &GrpcClient::close);
    godot::ClassDB::bind_method(godot::D_METHOD("is_connected"), &GrpcClient::is_connected);

    // Unary RPC
    godot::ClassDB::bind_method(godot::D_METHOD("unary", "full_method", "request_bytes", "call_opts"), &GrpcClient::unary, DEFVAL(godot::Dictionary()));

    // Server-streaming RPC
    godot::ClassDB::bind_method(godot::D_METHOD("server_stream_start", "full_method", "request_bytes", "call_opts"), &GrpcClient::server_stream_start, DEFVAL(godot::Dictionary()));
    godot::ClassDB::bind_method(godot::D_METHOD("server_stream_cancel", "stream_id"), &GrpcClient::server_stream_cancel);

    // Logging
    godot::ClassDB::bind_method(godot::D_METHOD("set_log_level", "level"), &GrpcClient::set_log_level);
    godot::ClassDB::bind_method(godot::D_METHOD("get_log_level"), &GrpcClient::get_log_level);

    // Signals for streaming
    ADD_SIGNAL(godot::MethodInfo("message", godot::PropertyInfo(godot::Variant::INT, "stream_id"), godot::PropertyInfo(godot::Variant::PACKED_BYTE_ARRAY, "data")));
    ADD_SIGNAL(godot::MethodInfo("finished", godot::PropertyInfo(godot::Variant::INT, "stream_id"), godot::PropertyInfo(godot::Variant::INT, "status_code"), godot::PropertyInfo(godot::Variant::STRING, "message")));
    ADD_SIGNAL(godot::MethodInfo("error", godot::PropertyInfo(godot::Variant::INT, "stream_id"), godot::PropertyInfo(godot::Variant::INT, "status_code"), godot::PropertyInfo(godot::Variant::STRING, "message")));
}

bool GrpcClient::connect(const godot::String& endpoint, const godot::Dictionary& options) {
    std::string endpoint_str = endpoint.utf8().get_data();
    ChannelOptions channel_opts = parse_channel_options(options);

    return channel_pool_.create_channel(endpoint_str, channel_opts);
}

void GrpcClient::close() {
    // Cancel all active streams
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        for (auto& pair : active_streams_) {
            pair.second->cancel();
        }
        active_streams_.clear();
    }

    // Close the channel
    channel_pool_.close();
}

bool GrpcClient::is_connected() const {
    return channel_pool_.is_connected();
}

godot::PackedByteArray GrpcClient::unary(
    const godot::String& full_method,
    const godot::PackedByteArray& request_bytes,
    const godot::Dictionary& call_opts
) {
    std::string method = full_method.utf8().get_data();
    Logger::debug("Unary call to " + method);

    auto stub = channel_pool_.get_stub();
    if (!stub) {
        Logger::error("No active connection for unary call");
        godot::UtilityFunctions::push_error("GrpcClient: Not connected");
        return godot::PackedByteArray();
    }

    // Create context
    auto context = create_context(call_opts);

    // Prepare request ByteBuffer
    grpc::ByteBuffer request_buffer;
    {
        const uint8_t* data = request_bytes.ptr();
        size_t size = request_bytes.size();
        grpc::Slice slice(data, size);
        request_buffer.Clear();
        grpc::ByteBuffer temp(&slice, 1);
        request_buffer.Swap(&temp);
    }

    // Use async API in blocking mode
    grpc::CompletionQueue cq;
    grpc::ByteBuffer response_buffer;
    grpc::Status status;

    std::unique_ptr<grpc::GenericClientAsyncResponseReader> rpc(
        stub->PrepareUnaryCall(context.get(), method, request_buffer, &cq)
    );

    rpc->StartCall();
    rpc->Finish(&response_buffer, &status, (void*)1);

    void* got_tag;
    bool ok = false;
    cq.Next(&got_tag, &ok);

    if (!ok || !status.ok()) {
        std::string error_msg = StatusMap::format_error(status);
        Logger::error("Unary call failed: " + error_msg);
        godot::UtilityFunctions::push_error(("GrpcClient: " + error_msg).c_str());
        return godot::PackedByteArray();
    }

    // Convert response ByteBuffer to PackedByteArray
    std::vector<grpc::Slice> slices;
    (void)response_buffer.Dump(&slices);

    godot::PackedByteArray response_bytes;
    for (const auto& slice : slices) {
        size_t slice_size = slice.size();
        int64_t old_size = response_bytes.size();
        response_bytes.resize(old_size + slice_size);
        memcpy(response_bytes.ptrw() + old_size, slice.begin(), slice_size);
    }

    Logger::debug("Unary call succeeded, response size: " + std::to_string(response_bytes.size()));
    return response_bytes;
}

int GrpcClient::server_stream_start(
    const godot::String& full_method,
    const godot::PackedByteArray& request_bytes,
    const godot::Dictionary& call_opts
) {
    std::string method = full_method.utf8().get_data();
    Logger::debug("Starting server stream for " + method);

    auto stub = channel_pool_.get_stub();
    if (!stub) {
        Logger::error("No active connection for server stream");
        godot::UtilityFunctions::push_error("GrpcClient: Not connected");
        return -1;
    }

    // Create context
    auto context = create_context(call_opts);

    // Allocate stream ID
    int stream_id;
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        stream_id = next_stream_id_++;
    }

    // Create stream with callbacks
    auto stream = std::make_unique<GrpcStream>(
        stream_id,
        stub,
        method,
        request_bytes,
        std::move(context),
        [this](int id, const godot::PackedByteArray& data) {
            this->on_stream_message(id, data);
        },
        [this](int id, int code, const std::string& msg) {
            this->on_stream_finished(id, code, msg);
        },
        [this](int id, int code, const std::string& msg) {
            this->on_stream_error(id, code, msg);
        }
    );

    // Start the stream
    stream->start();

    // Store the stream
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        active_streams_[stream_id] = std::move(stream);
    }

    Logger::info("Server stream " + std::to_string(stream_id) + " started");
    return stream_id;
}

void GrpcClient::server_stream_cancel(int stream_id) {
    std::lock_guard<std::mutex> lock(streams_mutex_);

    auto it = active_streams_.find(stream_id);
    if (it != active_streams_.end()) {
        Logger::debug("Cancelling server stream " + std::to_string(stream_id));
        it->second->cancel();
        active_streams_.erase(it);
    } else {
        Logger::warn("Stream " + std::to_string(stream_id) + " not found");
    }
}

void GrpcClient::set_log_level(int level) {
    Logger::set_level(static_cast<LogLevel>(level));
}

int GrpcClient::get_log_level() const {
    return static_cast<int>(Logger::get_level());
}

std::unique_ptr<grpc::ClientContext> GrpcClient::create_context(const godot::Dictionary& call_opts) {
    auto context = std::make_unique<grpc::ClientContext>();

    // Set deadline if specified
    if (call_opts.has("deadline_ms")) {
        int deadline_ms = call_opts["deadline_ms"];
        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_ms);
        context->set_deadline(deadline);
    }

    // Set metadata if specified
    if (call_opts.has("metadata")) {
        godot::Dictionary metadata = call_opts["metadata"];
        godot::Array keys = metadata.keys();
        for (int i = 0; i < keys.size(); ++i) {
            godot::String key = keys[i];
            godot::String value = metadata[key];
            context->AddMetadata(key.utf8().get_data(), value.utf8().get_data());
        }
    }

    return context;
}

ChannelOptions GrpcClient::parse_channel_options(const godot::Dictionary& options) {
    ChannelOptions opts;

    if (options.has("max_retries")) {
        opts.max_retries = options["max_retries"];
    }

    if (options.has("keepalive_seconds")) {
        opts.keepalive_seconds = options["keepalive_seconds"];
    }

    if (options.has("enable_tls")) {
        opts.enable_tls = options["enable_tls"];
    }

    if (options.has("authority")) {
        godot::String authority = options["authority"];
        opts.authority = authority.utf8().get_data();
    }

    if (options.has("max_send_message_length")) {
        opts.max_send_message_length = options["max_send_message_length"];
    }

    if (options.has("max_receive_message_length")) {
        opts.max_receive_message_length = options["max_receive_message_length"];
    }

    return opts;
}

void GrpcClient::on_stream_message(int stream_id, const godot::PackedByteArray& data) {
    Logger::trace("Stream " + std::to_string(stream_id) + " message callback");

    // Emit signal via call_deferred to ensure we're on the main thread
    call_deferred("emit_signal", "message", stream_id, data);
}

void GrpcClient::on_stream_finished(int stream_id, int status_code, const std::string& message) {
    Logger::trace("Stream " + std::to_string(stream_id) + " finished callback");

    // Clean up the stream
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        active_streams_.erase(stream_id);
    }

    // Emit signal via call_deferred
    godot::String msg(message.c_str());
    call_deferred("emit_signal", "finished", stream_id, status_code, msg);
}

void GrpcClient::on_stream_error(int stream_id, int status_code, const std::string& message) {
    Logger::trace("Stream " + std::to_string(stream_id) + " error callback");

    // Clean up the stream
    {
        std::lock_guard<std::mutex> lock(streams_mutex_);
        active_streams_.erase(stream_id);
    }

    // Emit signal via call_deferred
    godot::String msg(message.c_str());
    call_deferred("emit_signal", "error", stream_id, status_code, msg);
}

} // namespace godot_grpc
