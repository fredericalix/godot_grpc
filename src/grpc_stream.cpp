#include "grpc_stream.h"
#include "util/status_map.h"
#include <grpcpp/support/byte_buffer.h>
#include <grpcpp/support/slice.h>

namespace godot_grpc {

GrpcStream::GrpcStream(
    int stream_id,
    std::shared_ptr<grpc::GenericStub> stub,
    const std::string& method,
    const godot::PackedByteArray& request_bytes,
    std::unique_ptr<grpc::ClientContext> context,
    StreamMessageCallback on_message,
    StreamFinishedCallback on_finished,
    StreamErrorCallback on_error
)
    : stream_id_(stream_id),
      stub_(stub),
      method_(method),
      request_bytes_(request_bytes),
      context_(std::move(context)),
      on_message_(on_message),
      on_finished_(on_finished),
      on_error_(on_error),
      active_(false)
{
}

GrpcStream::~GrpcStream() {
    cancel();
    if (reader_thread_ && reader_thread_->joinable()) {
        reader_thread_->join();
    }
}

void GrpcStream::start() {
    if (active_.exchange(true)) {
        Logger::warn("Stream " + std::to_string(stream_id_) + " already started");
        return;
    }

    Logger::debug("Starting stream " + std::to_string(stream_id_) + " for method " + method_);
    reader_thread_ = std::make_unique<std::thread>(&GrpcStream::reader_thread, this);
}

void GrpcStream::cancel() {
    if (active_.load() && context_) {
        Logger::debug("Cancelling stream " + std::to_string(stream_id_));
        context_->TryCancel();
        active_.store(false);
    }
}

void GrpcStream::reader_thread() {
    Logger::trace("Reader thread started for stream " + std::to_string(stream_id_));

    // Prepare request ByteBuffer
    grpc::ByteBuffer request_buffer;
    {
        const uint8_t* data = request_bytes_.ptr();
        size_t size = request_bytes_.size();
        grpc::Slice slice(data, size);
        request_buffer.Clear();
        grpc::ByteBuffer temp(&slice, 1);
        request_buffer.Swap(&temp);
    }

    // Use async API with completion queue for server streaming
    grpc::CompletionQueue cq;

    std::unique_ptr<grpc::GenericClientAsyncReaderWriter> stream(
        stub_->PrepareCall(context_.get(), method_, &cq)
    );

    if (!stream) {
        Logger::error("Failed to prepare stream for method " + method_);
        if (on_error_) {
            on_error_(stream_id_, static_cast<int>(grpc::StatusCode::INTERNAL), "Failed to prepare stream");
        }
        active_.store(false);
        return;
    }

    // Start the call
    stream->StartCall((void*)1);

    void* got_tag;
    bool ok = false;
    cq.Next(&got_tag, &ok);

    if (!ok) {
        Logger::error("Failed to start stream");
        if (on_error_) {
            on_error_(stream_id_, static_cast<int>(grpc::StatusCode::INTERNAL), "Failed to start stream");
        }
        active_.store(false);
        return;
    }

    // Write the request
    stream->Write(request_buffer, (void*)2);
    cq.Next(&got_tag, &ok);

    if (!ok) {
        Logger::error("Failed to write request to stream");
        grpc::Status status;
        stream->Finish(&status, (void*)999);
        cq.Next(&got_tag, &ok);

        if (on_error_) {
            on_error_(stream_id_, static_cast<int>(status.error_code()), status.error_message());
        }
        active_.store(false);
        return;
    }

    // Signal writes are done
    stream->WritesDone((void*)3);
    cq.Next(&got_tag, &ok);

    // Read messages
    int read_tag = 100;
    while (active_.load()) {
        grpc::ByteBuffer response_buffer;
        stream->Read(&response_buffer, (void*)(intptr_t)read_tag);

        ok = false;
        cq.Next(&got_tag, &ok);

        if (!ok) {
            Logger::trace("Stream read completed, ending stream " + std::to_string(stream_id_));
            break;
        }

        // Convert ByteBuffer to PackedByteArray
        std::vector<grpc::Slice> slices;
        (void)response_buffer.Dump(&slices);

        godot::PackedByteArray response_bytes;
        for (const auto& slice : slices) {
            size_t slice_size = slice.size();
            int64_t old_size = response_bytes.size();
            response_bytes.resize(old_size + slice_size);
            memcpy(response_bytes.ptrw() + old_size, slice.begin(), slice_size);
        }

        Logger::trace("Stream " + std::to_string(stream_id_) + " received " + std::to_string(response_bytes.size()) + " bytes");

        if (on_message_) {
            on_message_(stream_id_, response_bytes);
        }

        read_tag++;
    }

    // Finish the call and get status
    grpc::Status status;
    stream->Finish(&status, (void*)999);
    cq.Next(&got_tag, &ok);

    Logger::debug("Stream " + std::to_string(stream_id_) + " finished with status: " +
                  StatusMap::status_code_string(status.error_code()));

    if (status.ok()) {
        if (on_finished_) {
            on_finished_(stream_id_, static_cast<int>(status.error_code()), status.error_message());
        }
    } else {
        std::string error_msg = StatusMap::format_error(status);
        Logger::error("Stream " + std::to_string(stream_id_) + " error: " + error_msg);
        if (on_error_) {
            on_error_(stream_id_, static_cast<int>(status.error_code()), status.error_message());
        }
    }

    active_.store(false);
}

} // namespace godot_grpc
