#ifndef GODOT_GRPC_CHANNEL_POOL_H
#define GODOT_GRPC_CHANNEL_POOL_H

#include <grpcpp/grpcpp.h>
#include <grpcpp/generic/generic_stub.h>
#include <memory>
#include <string>
#include <map>

namespace godot_grpc {

/**
 * Options for creating a gRPC channel.
 */
struct ChannelOptions {
    int max_retries = 0;
    int keepalive_seconds = 0;
    bool enable_tls = false;
    std::string authority;
    std::multimap<std::string, std::string> metadata;

    // Additional channel arguments
    int max_send_message_length = -1; // -1 = unlimited
    int max_receive_message_length = -1; // -1 = unlimited
};

/**
 * Manages gRPC channels and stubs.
 * Currently supports a single channel, but designed to support
 * pooling multiple channels in the future.
 */
class GrpcChannelPool {
public:
    GrpcChannelPool() = default;
    ~GrpcChannelPool() = default;

    /**
     * Create a channel to the specified endpoint.
     *
     * @param endpoint Target address (e.g., "dns:///localhost:50051", "ipv4:127.0.0.1:50051")
     * @param options Channel configuration options
     * @return true if the channel was created successfully
     */
    bool create_channel(const std::string& endpoint, const ChannelOptions& options);

    /**
     * Close the current channel and clean up resources.
     */
    void close();

    /**
     * Get the generic stub for making calls.
     * Returns nullptr if no channel is active.
     */
    std::shared_ptr<grpc::GenericStub> get_stub();

    /**
     * Check if a channel is active.
     */
    bool is_connected() const;

    /**
     * Get the current endpoint.
     */
    std::string get_endpoint() const { return endpoint_; }

private:
    std::shared_ptr<grpc::Channel> channel_;
    std::shared_ptr<grpc::GenericStub> stub_;
    std::string endpoint_;
};

} // namespace godot_grpc

#endif // GODOT_GRPC_CHANNEL_POOL_H
