#ifndef GODOT_GRPC_STATUS_MAP_H
#define GODOT_GRPC_STATUS_MAP_H

#include <grpcpp/grpcpp.h>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <string>

namespace godot_grpc {

/**
 * Maps gRPC status codes to Godot error codes and provides
 * utilities for error handling and logging.
 */
class StatusMap {
public:
    /**
     * Convert gRPC status code to Godot Error enum.
     */
    static int grpc_to_godot_error(grpc::StatusCode code);

    /**
     * Get human-readable string for gRPC status code.
     */
    static std::string status_code_string(grpc::StatusCode code);

    /**
     * Format a complete error message from gRPC status.
     */
    static std::string format_error(const grpc::Status& status);

    /**
     * Extract trailing metadata from a failed call context.
     */
    static std::string extract_trailing_metadata(const grpc::ClientContext& context);
};

/**
 * Log levels for the extension.
 */
enum class LogLevel {
    NONE = 0,
    ERR = 1,    // Renamed from ERROR to avoid Windows macro conflict
    WARN = 2,
    INFO = 3,
    DEBUG = 4,
    TRACE = 5
};

/**
 * Simple logger for the extension.
 */
class Logger {
public:
    static void set_level(LogLevel level);
    static LogLevel get_level();

    static void error(const std::string& message);
    static void warn(const std::string& message);
    static void info(const std::string& message);
    static void debug(const std::string& message);
    static void trace(const std::string& message);

private:
    static LogLevel current_level;
};

} // namespace godot_grpc

#endif // GODOT_GRPC_STATUS_MAP_H
