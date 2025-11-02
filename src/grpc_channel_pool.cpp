#include "grpc_channel_pool.h"
#include "util/status_map.h"

namespace godot_grpc {

bool GrpcChannelPool::create_channel(const std::string& endpoint, const ChannelOptions& options) {
    Logger::info("Creating gRPC channel to " + endpoint);

    // Build channel arguments
    grpc::ChannelArguments args;

    if (options.max_retries > 0) {
        args.SetInt(GRPC_ARG_MAX_RECONNECT_BACKOFF_MS, 5000);
        args.SetInt(GRPC_ARG_MIN_RECONNECT_BACKOFF_MS, 1000);
    }

    if (options.keepalive_seconds > 0) {
        args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, options.keepalive_seconds * 1000);
        args.SetInt(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 10000);
        args.SetInt(GRPC_ARG_KEEPALIVE_PERMIT_WITHOUT_CALLS, 1);
    }

    if (!options.authority.empty()) {
        args.SetString(GRPC_ARG_DEFAULT_AUTHORITY, options.authority);
    }

    if (options.max_send_message_length > 0) {
        args.SetMaxSendMessageSize(options.max_send_message_length);
    }

    if (options.max_receive_message_length > 0) {
        args.SetMaxReceiveMessageSize(options.max_receive_message_length);
    }

    // Create channel credentials
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (options.enable_tls) {
        grpc::SslCredentialsOptions ssl_opts;
        creds = grpc::SslCredentials(ssl_opts);
        Logger::info("Using TLS credentials");
    } else {
        creds = grpc::InsecureChannelCredentials();
        Logger::debug("Using insecure credentials");
    }

    // Create the channel
    channel_ = grpc::CreateCustomChannel(endpoint, creds, args);
    if (!channel_) {
        Logger::error("Failed to create channel to " + endpoint);
        return false;
    }

    // Create the generic stub
    stub_ = std::make_shared<grpc::GenericStub>(channel_);
    if (!stub_) {
        Logger::error("Failed to create generic stub");
        channel_.reset();
        return false;
    }

    endpoint_ = endpoint;
    Logger::info("Channel created successfully to " + endpoint);
    return true;
}

void GrpcChannelPool::close() {
    Logger::info("Closing gRPC channel to " + endpoint_);
    stub_.reset();
    channel_.reset();
    endpoint_.clear();
}

std::shared_ptr<grpc::GenericStub> GrpcChannelPool::get_stub() {
    return stub_;
}

bool GrpcChannelPool::is_connected() const {
    if (!channel_) {
        return false;
    }

    auto state = channel_->GetState(false);
    return state == GRPC_CHANNEL_READY || state == GRPC_CHANNEL_IDLE;
}

} // namespace godot_grpc
