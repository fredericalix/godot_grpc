#include "status_map.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <sstream>

namespace godot_grpc {

LogLevel Logger::current_level = LogLevel::WARN;

void Logger::set_level(LogLevel level) {
    current_level = level;
}

LogLevel Logger::get_level() {
    return current_level;
}

void Logger::error(const std::string& message) {
    if (current_level >= LogLevel::ERR) {
        godot::UtilityFunctions::push_error(("[GodotGRPC ERROR] " + message).c_str());
    }
}

void Logger::warn(const std::string& message) {
    if (current_level >= LogLevel::WARN) {
        godot::UtilityFunctions::push_warning(("[GodotGRPC WARN] " + message).c_str());
    }
}

void Logger::info(const std::string& message) {
    if (current_level >= LogLevel::INFO) {
        godot::UtilityFunctions::print(("[GodotGRPC INFO] " + message).c_str());
    }
}

void Logger::debug(const std::string& message) {
    if (current_level >= LogLevel::DEBUG) {
        godot::UtilityFunctions::print(("[GodotGRPC DEBUG] " + message).c_str());
    }
}

void Logger::trace(const std::string& message) {
    if (current_level >= LogLevel::TRACE) {
        godot::UtilityFunctions::print(("[GodotGRPC TRACE] " + message).c_str());
    }
}

int StatusMap::grpc_to_godot_error(grpc::StatusCode code) {
    switch (code) {
        case grpc::StatusCode::OK:
            return godot::OK;
        case grpc::StatusCode::CANCELLED:
            return godot::ERR_QUERY_FAILED;
        case grpc::StatusCode::UNKNOWN:
            return godot::ERR_BUG;
        case grpc::StatusCode::INVALID_ARGUMENT:
            return godot::ERR_INVALID_PARAMETER;
        case grpc::StatusCode::DEADLINE_EXCEEDED:
            return godot::ERR_TIMEOUT;
        case grpc::StatusCode::NOT_FOUND:
            return godot::ERR_DOES_NOT_EXIST;
        case grpc::StatusCode::ALREADY_EXISTS:
            return godot::ERR_ALREADY_EXISTS;
        case grpc::StatusCode::PERMISSION_DENIED:
            return godot::ERR_UNAUTHORIZED;
        case grpc::StatusCode::RESOURCE_EXHAUSTED:
            return godot::ERR_OUT_OF_MEMORY;
        case grpc::StatusCode::FAILED_PRECONDITION:
            return godot::ERR_INVALID_DATA;
        case grpc::StatusCode::ABORTED:
            return godot::ERR_BUSY;
        case grpc::StatusCode::OUT_OF_RANGE:
            return godot::ERR_PARAMETER_RANGE_ERROR;
        case grpc::StatusCode::UNIMPLEMENTED:
            return godot::ERR_UNAVAILABLE;
        case grpc::StatusCode::INTERNAL:
            return godot::ERR_BUG;
        case grpc::StatusCode::UNAVAILABLE:
            return godot::ERR_CANT_CONNECT;
        case grpc::StatusCode::DATA_LOSS:
            return godot::ERR_FILE_CORRUPT;
        case grpc::StatusCode::UNAUTHENTICATED:
            return godot::ERR_UNAUTHORIZED;
        default:
            return godot::FAILED;
    }
}

std::string StatusMap::status_code_string(grpc::StatusCode code) {
    switch (code) {
        case grpc::StatusCode::OK: return "OK";
        case grpc::StatusCode::CANCELLED: return "CANCELLED";
        case grpc::StatusCode::UNKNOWN: return "UNKNOWN";
        case grpc::StatusCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case grpc::StatusCode::DEADLINE_EXCEEDED: return "DEADLINE_EXCEEDED";
        case grpc::StatusCode::NOT_FOUND: return "NOT_FOUND";
        case grpc::StatusCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
        case grpc::StatusCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
        case grpc::StatusCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case grpc::StatusCode::FAILED_PRECONDITION: return "FAILED_PRECONDITION";
        case grpc::StatusCode::ABORTED: return "ABORTED";
        case grpc::StatusCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
        case grpc::StatusCode::UNIMPLEMENTED: return "UNIMPLEMENTED";
        case grpc::StatusCode::INTERNAL: return "INTERNAL";
        case grpc::StatusCode::UNAVAILABLE: return "UNAVAILABLE";
        case grpc::StatusCode::DATA_LOSS: return "DATA_LOSS";
        case grpc::StatusCode::UNAUTHENTICATED: return "UNAUTHENTICATED";
        default: return "UNKNOWN_STATUS_CODE";
    }
}

std::string StatusMap::format_error(const grpc::Status& status) {
    std::ostringstream oss;
    oss << "gRPC error [" << status_code_string(status.error_code())
        << " (" << static_cast<int>(status.error_code()) << ")]: "
        << status.error_message();

    if (!status.error_details().empty()) {
        oss << " | Details: " << status.error_details();
    }

    return oss.str();
}

std::string StatusMap::extract_trailing_metadata(const grpc::ClientContext& context) {
    std::ostringstream oss;
    const auto& metadata = context.GetServerTrailingMetadata();

    if (metadata.empty()) {
        return "";
    }

    oss << "Trailing metadata: ";
    bool first = true;
    for (const auto& pair : metadata) {
        if (!first) oss << ", ";
        oss << std::string(pair.first.data(), pair.first.size())
            << "="
            << std::string(pair.second.data(), pair.second.size());
        first = false;
    }

    return oss.str();
}

} // namespace godot_grpc
