#pragma once
#include "single_file_sender.h"
#include "util/data_block.h"
#include "util/hash.h"
#include <fstream>
#include <spdlog/spdlog.h>

namespace sender {

namespace {
template<typename Message>
void SetLastChunkFlag(Message& message, bool value) {
    if constexpr (requires(Message& msg) { msg.set_is_last_chunk(true); }) {
        message.set_is_last_chunk(value);
    } else {
        (void) message;
        (void) value;
    }
}
} // namespace

SingleFileSender::SingleFileSender(core::Executor& executor,
                                   core::net::io::Session& session,
                                   transfer::FileInfoRequest& file,
                                   const std::filesystem::path& absolute_path)
    : executor_(executor)
    , session_(session)
    , size_(file.size())
    , file_path_(absolute_path)
    , relative_path_(file.relative_path())
    , hash_(file.hash()) {}

asio::awaitable<void> SingleFileSender::send_file() {
    std::ifstream file(file_path_, std::ios::binary);
    if (!file) {
        spdlog::error("[SingleFileSender::send_file] Failed to open file: {}", file_path_.string());
        co_return;
    }

    std::vector<std::byte> buffer(kDefaultBufferSize);

    if (total_chunks_ == 0) {
        total_chunks_ = size_ == 0 ? 1 : (size_ + kDefaultChunkSize - 1) / kDefaultChunkSize;
        chunks_.reserve(static_cast<std::size_t>(total_chunks_));
    }

    uint64_t chunk_index = 0;
    uint64_t bytes_sent = 0;

    if (size_ == 0) {
        transfer::FileChunkRequest chunk_request;
        chunk_request.set_file_relative_path(relative_path_);
        chunk_request.set_chunk_index(0);
        chunk_request.set_data("", 0);
        chunk_request.set_hash(hash_);
        SetLastChunkFlag(chunk_request, true);

        chunks_.emplace_back();
        auto& chunk_info = chunks_.back();
        chunk_info.offset = 0;
        chunk_info.size = 0;
        chunk_info.hash = hash_;
        chunk_info.is_last = true;

        executor_.spawn(session_.send(chunk_request));

        spdlog::info("[SingleFileSender::send_file] File sent successfully: {}, {} chunks",
                     file_path_.string(),
                     1);
        co_return;
    }

    while (file && bytes_sent < size_) {
        file.read(reinterpret_cast<char*>(buffer.data()), kDefaultChunkSize);
        const auto bytes_read = static_cast<std::size_t>(file.gcount());

        if (bytes_read > 0) {
            bytes_sent += bytes_read;

            const bool is_last = bytes_sent >= size_;
            auto chunk_hash = util::hash::sha256_hex(ConstDataBlock(buffer.data(), bytes_read));

            transfer::FileChunkRequest chunk_request;
            chunk_request.set_file_relative_path(relative_path_);
            chunk_request.set_chunk_index(chunk_index);
            chunk_request.set_data(buffer.data(), bytes_read);
            chunk_request.set_hash(chunk_hash ? *chunk_hash : std::string());
            SetLastChunkFlag(chunk_request, is_last);

            chunks_.emplace_back();
            auto& chunk_info = chunks_.back();
            chunk_info.offset = bytes_sent - bytes_read;
            chunk_info.size = static_cast<std::uint32_t>(bytes_read);
            chunk_info.hash = chunk_hash ? *chunk_hash : std::string();
            chunk_info.is_last = is_last;

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

    auto& chunk = chunks_[chunk_index];
    if (chunk.status == ChunkInfo::Status::Completed) {
        co_return;
    }

    std::ifstream file(file_path_, std::ios::binary);
    if (!file) {
        spdlog::error("[SingleFileSender::send_chunk] Failed to open file: {}", file_path_.string());
        co_return;
    }

    file.seekg(static_cast<std::streamoff>(chunk.offset), std::ios::beg);
    std::vector<std::byte> buffer(chunk.size);
    file.read(reinterpret_cast<char*>(buffer.data()), chunk.size);
    const auto bytes_read = static_cast<std::size_t>(file.gcount());

    if (bytes_read > 0) {
        auto chunk_hash = chunk.hash;
        if (chunk_hash.empty()) {
            auto computed_hash = util::hash::sha256_hex(ConstDataBlock(buffer.data(), bytes_read));
            if (computed_hash) {
                chunk_hash = *computed_hash;
                chunk.hash = chunk_hash;
            }
        }

        transfer::FileChunkRequest chunk_request;
        chunk_request.set_file_relative_path(relative_path_);
        chunk_request.set_chunk_index(chunk_index);
        chunk_request.set_data(buffer.data(), bytes_read);
        chunk_request.set_hash(chunk_hash);
        SetLastChunkFlag(chunk_request, chunk.is_last);

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
        if (chunk.status != ChunkInfo::Status::Completed) {
            chunk.status = ChunkInfo::Status::Completed;
            ++completed_chunks_;
            spdlog::debug(
                "[SingleFileSender::update_chunk_status] Chunk {} of file {} marked as completed",
                chunk_index,
                file_path_.string());

            if (completed_chunks_ == total_chunks_ && total_chunks_ != 0 && !completion_announced_) {
                completion_announced_ = true;
                spdlog::info(
                    "[SingleFileSender::update_chunk_status] All chunks acknowledged for file {}",
                    file_path_.string());
            }
        }
    } else {
        chunk.status = ChunkInfo::Status::Failed;
        spdlog::warn("[SingleFileSender::update_chunk_status] Chunk {} of file {} marked as failed",
                     chunk_index,
                     file_path_.string());
    }
}
} // namespace sender