#include "session.h"
#include "asio/awaitable.hpp"
#include "single_file_receiver.h"
#include "transfer.pb.h"
#include "util/data_block.h"
#include <asio/use_awaitable.hpp>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace std::literals;

namespace receiver {

constexpr uint64_t kBufferSize = 1024 * 1024 + 512;

Session::Session(core::Executor& executor)
    : executor_(executor)
    , socket_(executor_.get_io_context())
    , tcp_receiver_(executor_, socket_, kListeningPort) {}

asio::awaitable<std::optional<transfer::TransferMetadataRequest>> Session::receive_metadata(
    std::vector<std::byte>& buffer) {
    MutDataBlock buf_span(buffer.data(), buffer.size());
    co_await tcp_receiver_.receive(buf_span);

    if (buf_span.empty()) {
        co_return std::nullopt;
    }

    std::string serialized_metadata(reinterpret_cast<const char*>(buf_span.data()), buf_span.size());
    transfer::TransferMetadataRequest metadata;
    if (!metadata.ParseFromString(serialized_metadata)) {
        spdlog::error("Failed to parse TransferMetadataRequest");
        co_return std::nullopt;
    }

    co_return metadata;
}

std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>> Session::create_receivers(
    const transfer::TransferMetadataRequest& metadata,
    std::vector<transfer::FileInfoRequest>& file_infos) {
    file_infos.reserve(metadata.files_size());
    std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>> receivers;

    for (int i = 0; i < metadata.files_size(); ++i) {
        const auto& fi = metadata.files(i);
        file_infos.push_back(fi);
        auto& stored = file_infos.back();
        receivers.emplace(stored.relative_path(), std::make_unique<SingleFileReceiver>(stored));
    }

    return receivers;
}

asio::awaitable<std::optional<transfer::FileChunkRequest>> Session::receive_chunk(
    std::vector<std::byte>& buffer) {
    MutDataBlock buf_span(buffer.data(), buffer.size());
    co_await tcp_receiver_.receive(buf_span);

    if (buf_span.empty()) {
        co_return std::nullopt; // connection closed
    }

    std::string chunk_serialized(reinterpret_cast<const char*>(buf_span.data()), buf_span.size());
    transfer::FileChunkRequest chunk;
    if (!chunk.ParseFromString(chunk_serialized)) {
        spdlog::warn("Failed to parse FileChunkRequest");
        co_return std::nullopt;
    }

    co_return chunk;
}

void Session::dispatch_chunk(
    const transfer::FileChunkRequest& chunk,
    std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>>& receivers) {
    const std::string& rel = chunk.file_relative_path();
    auto it = receivers.find(rel);
    if (it == receivers.end()) {
        spdlog::warn("No receiver found for file: {}", rel);
        return;
    }

    // make a local copy of chunk and spawn processing the chunk
    transfer::FileChunkRequest local_chunk = chunk;
    executor_.spawn([receiver = it->second.get(),
                     local_chunk = std::move(local_chunk)]() mutable -> asio::awaitable<void> {
        co_await receiver->receive_chunk(const_cast<transfer::FileChunkRequest&>(local_chunk));
    });
}

asio::awaitable<void> Session::receive() {
    // 1. 接收 TransferMetadataRequest
    std::vector<std::byte> buffer(kBufferSize);

    auto metadata_opt = co_await receive_metadata(buffer);
    if (!metadata_opt) {
        co_return;
    }
    auto& metadata = *metadata_opt;

    // 2. 根据 metadata 创建 SingleFileReceiver
    std::vector<transfer::FileInfoRequest> file_infos;
    auto receivers = create_receivers(metadata, file_infos);

    // track completed files
    std::unordered_set<std::string> completed_files;

    // 3. 接受 FileChunkRequest 并分发
    while (true) {
        auto chunk_opt = co_await receive_chunk(buffer);
        if (!chunk_opt) {
            break; // connection closed or parse error
        }
        auto& chunk = *chunk_opt;

        dispatch_chunk(chunk, receivers);

        if (chunk.is_last_chunk()) {
            completed_files.insert(chunk.file_relative_path());
            if (completed_files.size() >= static_cast<size_t>(metadata.files_size())) {
                break; // all files done
            }
        }
    }
    co_return;
}
} // namespace receiver