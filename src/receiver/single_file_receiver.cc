#include "single_file_receiver.h"
#include "util/data_block.h"
#include "util/hash.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace receiver {

namespace {
template<typename Message>
bool GetLastChunkFlag(const Message& message) {
    if constexpr (requires(const Message& msg) { msg.is_last_chunk(); }) {
        return message.is_last_chunk();
    }
    return false;
}
} // namespace

SingleFileReceiver::SingleFileReceiver(std::string relative_path,
                                       std::string expected_file_hash,
                                       std::uint64_t file_size)
    : rel_path_(std::move(relative_path))
    , expected_hash_(std::move(expected_file_hash))
    , file_size_(file_size) {
    expected_total_chunks_ = file_size_ == 0
                                 ? 1
                                 : (file_size_ + kDefaultChunkSize - 1) / kDefaultChunkSize;
    chunks_.resize(static_cast<std::size_t>(expected_total_chunks_));
}

bool SingleFileReceiver::prepare_storage(const std::filesystem::path& dest_path) {
    dest_path_ = dest_path;

    std::error_code ec;
    const auto parent = dest_path_.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            spdlog::error(
                "[SingleFileReceiver::prepare_storage] Failed to create directories for {}: {}",
                dest_path_.string(),
                ec.message());
            return false;
        }
    }

    fs_.open(dest_path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!fs_.is_open()) {
        spdlog::error("[SingleFileReceiver::prepare_storage] Failed to open file: {}",
                      dest_path_.string());
        return false;
    }

    storage_prepared_ = true;
    bytes_received_ = 0;
    completed_chunks_ = 0;
    last_chunk_received_ = false;
    finalized_ = false;
    std::fill(chunks_.begin(), chunks_.end(), ChunkInfo{});
    return true;
}

bool SingleFileReceiver::is_complete() const {
    if (finalized_) {
        return true;
    }

    const bool chunks_received = expected_total_chunks_ > 0
                                 && completed_chunks_ >= expected_total_chunks_;
    const bool bytes_matched = file_size_ == 0 ? completed_chunks_ > 0
                                               : bytes_received_ >= file_size_;
    
    spdlog::debug("[SingleFileReceiver::is_complete] {} - completed_chunks_={}, expected_total_chunks_={}, "
                  "bytes_received_={}, file_size_={}, last_chunk_received_={}, chunks_received={}, bytes_matched={}",
                  rel_path_, completed_chunks_, expected_total_chunks_, bytes_received_, file_size_, 
                  last_chunk_received_, chunks_received, bytes_matched);
    
    if (file_size_ == 0) {
        return chunks_received || last_chunk_received_;
    }

    return chunks_received && (bytes_matched || last_chunk_received_);
}

bool SingleFileReceiver::handle_chunk(const transfer::FileChunkRequest& request) {
    if (request.file_relative_path() != rel_path_) {
        spdlog::warn(
            "[SingleFileReceiver::handle_chunk] Mismatched relative path: {} (expected {})",
            request.file_relative_path(),
            rel_path_);
        return false;
    }

    if (!storage_prepared_) {
        if (dest_path_.empty()) {
            dest_path_ = std::filesystem::path(rel_path_);
        }

        if (!prepare_storage(dest_path_)) {
            return false;
        }
    } else if (!fs_.is_open()) {
        fs_.open(dest_path_, std::ios::binary | std::ios::in | std::ios::out);
        if (!fs_.is_open()) {
            spdlog::error("[SingleFileReceiver::handle_chunk] Failed to reopen file: {}",
                          dest_path_.string());
            return false;
        }
    }

    const std::uint64_t chunk_index = request.chunk_index();
    if (chunk_index >= chunks_.size()) {
        chunks_.resize(static_cast<std::size_t>(chunk_index + 1));
        expected_total_chunks_ = chunks_.size();
    }

    auto& chunk_info = chunks_[static_cast<std::size_t>(chunk_index)];
    if (chunk_info.status == ChunkInfo::Status::Completed) {
        return true;
    }

    const auto& data = request.data();
    const bool is_last_chunk = GetLastChunkFlag(request);
    ConstDataBlock data_block(reinterpret_cast<const std::byte*>(data.data()), data.size());

    spdlog::debug("[SingleFileReceiver::handle_chunk] {} chunk {} - size={}, is_last={}", 
                  rel_path_, chunk_index, data.size(), is_last_chunk);

    if (!request.hash().empty()) {
        auto computed_hash = util::hash::sha256_hex(data_block);
        if (!computed_hash || *computed_hash != request.hash()) {
            chunk_info.status = ChunkInfo::Status::Failed;
            spdlog::warn("[SingleFileReceiver::handle_chunk] Hash mismatch for {} chunk {}",
                         rel_path_,
                         chunk_index);
            return false;
        }
        chunk_info.hash = *computed_hash;
    } else {
        chunk_info.hash.clear();
    }

    const std::uint64_t offset = chunk_index * kDefaultChunkSize;
    fs_.clear();
    fs_.seekp(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!fs_) {
        spdlog::error("[SingleFileReceiver::handle_chunk] Failed to seek to offset {} in {}",
                      offset,
                      dest_path_.string());
        return false;
    }

    if (!data.empty()) {
        fs_.write(data.data(), static_cast<std::streamsize>(data.size()));
        if (!fs_) {
            spdlog::error("[SingleFileReceiver::handle_chunk] Failed to write chunk {} for file {}",
                          chunk_index,
                          dest_path_.string());
            return false;
        }
    }

    fs_.flush();

    chunk_info.offset = offset;
    chunk_info.size = static_cast<std::uint32_t>(data.size());
    chunk_info.is_last = is_last_chunk;
    chunk_info.status = ChunkInfo::Status::Completed;

    bytes_received_ += static_cast<std::uint64_t>(data.size());
    last_chunk_received_ = last_chunk_received_ || is_last_chunk;
    ++completed_chunks_;

    if (is_last_chunk && expected_total_chunks_ < chunk_index + 1) {
        expected_total_chunks_ = chunk_index + 1;
    }

    return true;
}

std::tuple<bool, std::string, std::string> SingleFileReceiver::finalize_and_verify() {
    if (fs_.is_open()) {
        fs_.flush();
        fs_.close();
    }

    finalized_ = true;

    auto actual_hash_opt = util::hash::sha256_file_hex(dest_path_);
    std::string actual_hash = actual_hash_opt.value_or(std::string());

    bool chunk_count_ok = expected_total_chunks_ == 0
                          || completed_chunks_ >= expected_total_chunks_;
    if (file_size_ != 0 && bytes_received_ < file_size_) {
        chunk_count_ok = false;
    }

    const bool hash_ok = expected_hash_.empty()
                         || (actual_hash_opt && actual_hash == expected_hash_);
    const bool success = chunk_count_ok && hash_ok;

    if (!success) {
        if (!chunk_count_ok) {
            spdlog::warn(
                "[SingleFileReceiver::finalize_and_verify] Incomplete file {} (chunks: {}/{})",
                rel_path_,
                completed_chunks_,
                expected_total_chunks_);
        }
        if (!hash_ok) {
            spdlog::warn("[SingleFileReceiver::finalize_and_verify] Hash mismatch for {} (expected "
                         "{}, actual {})",
                         rel_path_,
                         expected_hash_,
                         actual_hash);
        }
    }

    return {success, expected_hash_, actual_hash};
}

} // namespace receiver
