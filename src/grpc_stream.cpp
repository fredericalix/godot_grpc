#include "grpc_stream.h"
#include "util/status_map.h"
#include <grpcpp/support/byte_buffer.h>
#include <grpcpp/support/slice.h>

namespace godot_grpc {

GrpcStream::GrpcStream(
    int stream_id,
    StreamType stream_type,
    std::shared_ptr<grpc::GenericStub> stub,
    const std::string& method,
    const godot::PackedByteArray& request_bytes,
    std::unique_ptr<grpc::ClientContext> context,
    StreamMessageCallback on_message,
    StreamFinishedCallback on_finished,
    StreamErrorCallback on_error
)
    : stream_id_(stream_id),
      stream_type_(stream_type),
      stub_(stub),
      method_(method),
      initial_request_bytes_(request_bytes),
      context_(std::move(context)),
      on_message_(on_message),
      on_finished_(on_finished),
      on_error_(on_error),
      active_(false),
      writes_done_(false),
      write_queue_closed_(false),
      cq_(std::make_shared<grpc::CompletionQueue>())
{
}

GrpcStream::~GrpcStream() {
    cancel();

    // Close write queue to unblock writer thread
    {
        std::lock_guard<std::mutex> lock(write_queue_mutex_);
        write_queue_closed_ = true;
    }
    write_queue_cv_.notify_all();

    // Join threads
    if (writer_thread_ && writer_thread_->joinable()) {
        writer_thread_->join();
    }
    if (reader_thread_ && reader_thread_->joinable()) {
        reader_thread_->join();
    }

    // Shutdown completion queue
    if (cq_) {
        cq_->Shutdown();
    }
}

void GrpcStream::start() {
    if (active_.exchange(true)) {
        Logger::warn("Stream " + std::to_string(stream_id_) + " already started");
        return;
    }

    std::string stream_type_str =
        (stream_type_ == StreamType::SERVER_STREAMING ? "server-streaming" :
         stream_type_ == StreamType::CLIENT_STREAMING ? "client-streaming" : "bidirectional");
    Logger::debug("Starting " + stream_type_str +
        " stream " + std::to_string(stream_id_) + " for method " + method_);

    // Prepare the stream
    stream_ = std::shared_ptr<grpc::GenericClientAsyncReaderWriter>(
        stub_->PrepareCall(context_.get(), method_, cq_.get())
    );

    if (!stream_) {
        Logger::error("Failed to prepare stream for method " + method_);
        if (on_error_) {
            on_error_(stream_id_, static_cast<int>(grpc::StatusCode::INTERNAL), "Failed to prepare stream");
        }
        active_.store(false);
        return;
    }

    // Start call
    stream_->StartCall((void*)1);

    void* got_tag;
    bool ok = false;
    cq_->Next(&got_tag, &ok);

    if (!ok) {
        Logger::error("Failed to start stream");
        if (on_error_) {
            on_error_(stream_id_, static_cast<int>(grpc::StatusCode::INTERNAL), "Failed to start stream");
        }
        active_.store(false);
        return;
    }

    // For server-streaming, write the initial request immediately and close writes
    if (stream_type_ == StreamType::SERVER_STREAMING && initial_request_bytes_.size() > 0) {
        grpc::ByteBuffer request_buffer;
        const uint8_t* data = initial_request_bytes_.ptr();
        size_t size = initial_request_bytes_.size();
        grpc::Slice slice(data, size);
        request_buffer.Clear();
        grpc::ByteBuffer temp(&slice, 1);
        request_buffer.Swap(&temp);

        stream_->Write(request_buffer, (void*)2);
        cq_->Next(&got_tag, &ok);

        if (!ok) {
            Logger::error("Failed to write initial request to server stream");
            grpc::Status status;
            stream_->Finish(&status, (void*)999);
            cq_->Next(&got_tag, &ok);

            if (on_error_) {
                on_error_(stream_id_, static_cast<int>(status.error_code()), status.error_message());
            }
            active_.store(false);
            return;
        }

        stream_->WritesDone((void*)3);
        cq_->Next(&got_tag, &ok);
        writes_done_.store(true);
    }
    // For client-streaming and bidirectional, queue initial request if present
    else if (initial_request_bytes_.size() > 0) {
        std::lock_guard<std::mutex> lock(write_queue_mutex_);
        write_queue_.push(initial_request_bytes_);
        write_queue_cv_.notify_one();
    }

    // Spawn reader thread
    reader_thread_ = std::make_unique<std::thread>(&GrpcStream::reader_thread, this);

    // Spawn writer thread for client-streaming and bidirectional
    if (stream_type_ == StreamType::CLIENT_STREAMING || stream_type_ == StreamType::BIDIRECTIONAL) {
        writer_thread_ = std::make_unique<std::thread>(&GrpcStream::writer_thread, this);
    }
}

void GrpcStream::cancel() {
    if (active_.load() && context_) {
        Logger::debug("Cancelling stream " + std::to_string(stream_id_));
        context_->TryCancel();
        active_.store(false);
    }
}

bool GrpcStream::send(const godot::PackedByteArray& message_bytes) {
    if (!active_.load()) {
        Logger::warn("Cannot send on inactive stream " + std::to_string(stream_id_));
        return false;
    }

    if (stream_type_ == StreamType::SERVER_STREAMING) {
        Logger::error("Cannot send on server-streaming stream " + std::to_string(stream_id_));
        return false;
    }

    if (writes_done_.load()) {
        Logger::warn("Cannot send on stream " + std::to_string(stream_id_) + " after WritesDone");
        return false;
    }

    std::lock_guard<std::mutex> lock(write_queue_mutex_);
    if (write_queue_closed_) {
        return false;
    }

    write_queue_.push(message_bytes);
    write_queue_cv_.notify_one();

    Logger::trace("Queued message for stream " + std::to_string(stream_id_) +
                  ", queue size: " + std::to_string(write_queue_.size()));
    return true;
}

void GrpcStream::close_send() {
    if (stream_type_ == StreamType::SERVER_STREAMING) {
        Logger::debug("close_send called on server-streaming stream (already done)");
        return;
    }

    Logger::debug("Closing send side of stream " + std::to_string(stream_id_));

    {
        std::lock_guard<std::mutex> lock(write_queue_mutex_);
        write_queue_closed_ = true;
    }
    write_queue_cv_.notify_all();
}

void GrpcStream::writer_thread() {
    Logger::trace("Writer thread started for stream " + std::to_string(stream_id_));

    int write_tag = 1000;

    while (active_.load()) {
        godot::PackedByteArray message_bytes;

        // Wait for messages in the queue
        {
            std::unique_lock<std::mutex> lock(write_queue_mutex_);
            write_queue_cv_.wait(lock, [this] {
                return !write_queue_.empty() || write_queue_closed_;
            });

            if (write_queue_.empty()) {
                if (write_queue_closed_) {
                    // No more messages to write
                    break;
                }
                continue;
            }

            message_bytes = write_queue_.front();
            write_queue_.pop();
        }

        // Prepare ByteBuffer
        grpc::ByteBuffer write_buffer;
        {
            const uint8_t* data = message_bytes.ptr();
            size_t size = message_bytes.size();
            grpc::Slice slice(data, size);
            write_buffer.Clear();
            grpc::ByteBuffer temp(&slice, 1);
            write_buffer.Swap(&temp);
        }

        // Write to stream
        stream_->Write(write_buffer, (void*)(intptr_t)write_tag);

        void* got_tag;
        bool ok = false;
        cq_->Next(&got_tag, &ok);

        if (!ok) {
            Logger::error("Failed to write message to stream " + std::to_string(stream_id_));
            // Don't call error callback here, reader thread will handle final status
            break;
        }

        Logger::trace("Wrote message to stream " + std::to_string(stream_id_));
        write_tag++;
    }

    // Signal writes are done
    if (active_.load() && !writes_done_.exchange(true)) {
        Logger::debug("Calling WritesDone on stream " + std::to_string(stream_id_));
        stream_->WritesDone((void*)9999);

        void* got_tag;
        bool ok = false;
        cq_->Next(&got_tag, &ok);
    }

    Logger::trace("Writer thread finished for stream " + std::to_string(stream_id_));
}

void GrpcStream::reader_thread() {
    Logger::trace("Reader thread started for stream " + std::to_string(stream_id_));

    // Read messages
    int read_tag = 10000;
    while (active_.load()) {
        grpc::ByteBuffer response_buffer;
        stream_->Read(&response_buffer, (void*)(intptr_t)read_tag);

        void* got_tag;
        bool ok = false;
        cq_->Next(&got_tag, &ok);

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

        Logger::trace("Stream " + std::to_string(stream_id_) + " received " +
                      std::to_string(response_bytes.size()) + " bytes");

        if (on_message_) {
            on_message_(stream_id_, response_bytes);
        }

        read_tag++;
    }

    // Finish the call and get status
    grpc::Status status;
    stream_->Finish(&status, (void*)99999);

    void* got_tag;
    bool ok = false;
    cq_->Next(&got_tag, &ok);

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
    Logger::trace("Reader thread finished for stream " + std::to_string(stream_id_));
}

} // namespace godot_grpc
