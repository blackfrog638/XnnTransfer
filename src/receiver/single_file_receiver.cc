#include "single_file_receiver.h"
#include "util/data_block.h"
#include "util/hash.h"
#include <spdlog/spdlog.h>

namespace receiver {

SingleFileReceiver::SingleFileReceiver(std::string relative_path,
                                       std::string expected_file_hash,
                                       std::uint64_t file_size)
    : rel_path_(std::move(relative_path))
    , expected_hash_(std::move(expected_file_hash))
    , file_size_(file_size) {
    // store under ./received/<relative_path>
    dest_path_ = std::filesystem::current_path() / "received" / rel_path_;
    auto parent = dest_path_.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::filesystem::create_directories(parent);
    }

    fs_.open(dest_path_, std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    if (!fs_.is_open()) {
        spdlog::error("[SingleFileReceiver] Failed to open {} for writing", dest_path_.string());
        valid_ = false;
        return;
    }
    if (file_size_ > 0) {
        fs_.seekp(static_cast<std::streamoff>(file_size_ - 1));
        fs_.put(0);
        fs_.seekp(0);
        expected_total_chunks_ = (file_size_ + kDefaultChunkSize - 1) / kDefaultChunkSize;
        spdlog::debug("[SingleFileReceiver] Preallocated {} bytes, expecting {} chunks",
                      file_size_,
                      expected_total_chunks_);
    }

    valid_ = true;
}

bool SingleFileReceiver::handle_chunk(const transfer::FileChunkRequest& request) {
    if (!valid_)
        return false;

    std::uint64_t chunk_index = request.chunk_index();

    if (received_chunks_.count(chunk_index) > 0) {
        spdlog::warn("[SingleFileReceiver] Duplicate chunk {} for {}, ignoring",
                     chunk_index,
                     rel_path_);
        return true;
    }

    const std::string& data = request.data();
    const std::byte* bytes = reinterpret_cast<const std::byte*>(data.data());
    ConstDataBlock block(bytes, data.size());

    if (!request.hash().empty()) {
        auto hex = util::hash::sha256_hex(block);
        if (!hex.has_value() || *hex != request.hash()) {
            spdlog::error(
                "[SingleFileReceiver] Chunk hash mismatch for {} chunk {}: expected {}, got {}",
                rel_path_,
                chunk_index,
                request.hash(),
                hex ? *hex : std::string("(none)"));
            return false;
        }
    }

    std::uint64_t offset = chunk_index * kDefaultChunkSize;

    fs_.seekp(static_cast<std::streamoff>(offset));
    if (!fs_) {
        spdlog::error("[SingleFileReceiver] Failed to seek to position {} for chunk {} in {}",
                      offset,
                      chunk_index,
                      rel_path_);
        return false;
    }

    fs_.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(data.size()));
    if (!fs_) {
        spdlog::error("[SingleFileReceiver] Failed to write chunk {} for {}",
                      chunk_index,
                      rel_path_);
        return false;
    }

    received_chunks_.insert(chunk_index);

    spdlog::debug("[SingleFileReceiver] Received chunk {} for {} at offset {} ({} bytes) [{}/{}]",
                  chunk_index,
                  rel_path_,
                  offset,
                  data.size(),
                  received_chunks_.size(),
                  expected_total_chunks_);

    if (request.is_last_chunk()) {
        last_chunk_received_ = true;
        expected_total_chunks_ = chunk_index + 1;
        spdlog::info("[SingleFileReceiver] Last chunk {} received for {}, total chunks: {}",
                     chunk_index,
                     rel_path_,
                     expected_total_chunks_);
    }

    return true;
}

std::tuple<bool, std::string, std::string> SingleFileReceiver::finalize_and_verify() {
    if (!last_chunk_received_) {
        spdlog::warn("[SingleFileReceiver] Last chunk not received for {}", rel_path_);
        if (fs_.is_open()) {
            fs_.close();
        }
        return {false, expected_hash_, std::string()};
    }
    if (expected_total_chunks_ > 0 && received_chunks_.size() != expected_total_chunks_) {
        spdlog::error("[SingleFileReceiver] Missing chunks for {}: received {}/{} chunks",
                      rel_path_,
                      received_chunks_.size(),
                      expected_total_chunks_);

        for (std::uint64_t i = 0; i < expected_total_chunks_; ++i) {
            if (received_chunks_.count(i) == 0) {
                spdlog::error("[SingleFileReceiver] Missing chunk {} for {}", i, rel_path_);
            }
        }

        if (fs_.is_open()) {
            fs_.close();
        }
        return {false, expected_hash_, std::string()};
    }

    if (fs_.is_open()) {
        fs_.flush();
        fs_.close();
    }

    std::optional<std::string> actual = util::hash::sha256_file_hex(dest_path_);
    std::string act = actual ? *actual : std::string();
    bool ok = true;
    if (!expected_hash_.empty()) {
        ok = (actual.has_value() && *actual == expected_hash_);
        if (ok) {
            spdlog::info("[SingleFileReceiver] File {} verified successfully", rel_path_);
        } else {
            spdlog::error("[SingleFileReceiver] Hash mismatch for {}: expected {}, got {}",
                          rel_path_,
                          expected_hash_,
                          act);
        }
    }

    return {ok, expected_hash_, act};
}

} // namespace receiver
