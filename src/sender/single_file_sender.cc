#pragma once
#include "single_file_sender.h"
#include "util/data_block.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace sender {

SingleFileSender::SingleFileSender(core::Executor& executor,
                                   core::net::io::Session& session,
                                   transfer::FileInfoRequest& file)
    : executor_(executor)
    , session_(session)
    , size_(file.size())
    , file_path_(file.relative_path())
    , hash_(file.hash()) {}

asio::awaitable<void> SingleFileSender::send_file() {
    std::ifstream file(file_path_, std::ios::binary);
    if (!file) {
        spdlog::error("[SingleFileSender::send_file] Failed to open file: {}", file_path_.string());
        co_return;
    }

    std::vector<std::byte> buffer(kDefaultBufferSize);

    uint64_t chunk_index = 0;
    uint64_t bytes_sent = 0;

    while (file && bytes_sent < size_) {
        file.read(reinterpret_cast<char*>(buffer.data()), kDefaultBufferSize);
        const auto bytes_read = static_cast<std::size_t>(file.gcount());

        if (bytes_read > 0) {
            transfer::FileChunkRequest chunk_request;
            chunk_request.set_file_relative_path(file_path_.string());
            chunk_request.set_chunk_index(chunk_index);
            chunk_request.set_data(buffer.data(), bytes_read);
            chunk_request.set_hash(hash_);

            bytes_sent += bytes_read;
            chunk_request.set_is_last_chunk(bytes_sent >= size_);

            chunks_.emplace_back(ChunkInfo::Status::InProgress, bytes_sent - bytes_read, bytes_read);

            executor_.spawn(session_.send(chunk_request));

            chunk_index++;
        }
    }

    if (file.bad()) {
        spdlog::error("[SingleFileSender::send_file] Error reading file: {}", file_path_.string());
        co_return;
    }

    spdlog::info("[SingleFileSender::send_file] File sent successfully: {}, {} chunks",
                 file_path_.string(),
                 chunk_index);
    co_return;
}

asio::awaitable<void> SingleFileSender::send_chunk(std::uint64_t chunk_index) {
    if (chunk_index >= chunks_.size()) {
        co_return;
    }

    const auto& chunk = chunks_[chunk_index];
    if (chunk.status == ChunkInfo::Status::Completed) {
        co_return;
    }

    std::ifstream file(file_path_, std::ios::binary);
    if (!file) {
        spdlog::error("[SingleFileSender::send_chunk] Failed to open file: {}", file_path_.string());
        co_return;
    }

    file.seekg(chunk.offset);
    std::vector<std::byte> buffer(chunk.size);
    file.read(reinterpret_cast<char*>(buffer.data()), chunk.size);
    const auto bytes_read = static_cast<std::size_t>(file.gcount());

    if (bytes_read > 0) {
        transfer::FileChunkRequest chunk_request;
        chunk_request.set_file_relative_path(file_path_.string());
        chunk_request.set_chunk_index(chunk_index);
        chunk_request.set_data(buffer.data(), bytes_read);
        chunk_request.set_hash(hash_);

        executor_.spawn(session_.send(chunk_request));
    }

    co_return;
}

void SingleFileSender::update_chunk_status(std::uint64_t chunk_index, bool success) {
    if (chunk_index >= chunks_.size()) {
        spdlog::warn("[SingleFileSender::update_chunk_status] Invalid chunk index {} for file {}",
                     chunk_index,
                     file_path_.string());
        return;
    }

    auto& chunk = chunks_[chunk_index];
    if (success) {
        chunk.status = ChunkInfo::Status::Completed;
        spdlog::debug(
            "[SingleFileSender::update_chunk_status] Chunk {} of file {} marked as completed",
            chunk_index,
            file_path_.string());
    } else {
        chunk.status = ChunkInfo::Status::Failed;
        spdlog::warn("[SingleFileSender::update_chunk_status] Chunk {} of file {} marked as failed",
                     chunk_index,
                     file_path_.string());
    }
}
} // namespace sender