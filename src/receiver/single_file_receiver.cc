#include "single_file_receiver.h"
#include "util/data_block.h"
#include "util/hash.h"
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace {
constexpr uint64_t kChunkSize = 1 * 1024 * 1024;
}

namespace receiver {

SingleFileReceiver::SingleFileReceiver(transfer::FileInfoRequest& file_info)
    : file_info_(file_info) {}

asio::awaitable<bool> SingleFileReceiver::prepare_file(const std::filesystem::path& dest_path) {
    const auto parent = dest_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            spdlog::error("Failed to create directories for {}: {}", parent.string(), ec.message());
            co_return false;
        }
    }

    // Ensure file exists
    if (!std::filesystem::exists(dest_path)) {
        std::ofstream create(dest_path, std::ios::binary);
        if (!create.is_open()) {
            spdlog::error("Failed to create file: {}", dest_path.string());
            co_return false;
        }
        create.close();
    }

    co_return true;
}

asio::awaitable<bool> SingleFileReceiver::write_chunk_data(
    const std::filesystem::path& dest_path, const transfer::FileChunkRequest& chunk_request) {
    std::fstream file(dest_path, std::ios::in | std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("Failed to open file for writing: {}", dest_path.string());
        co_return false;
    }

    const uint64_t offset = chunk_request.chunk_index() * kChunkSize;
    file.seekp(static_cast<std::streamoff>(offset));
    const std::string& data = chunk_request.data();
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    file.flush();
    file.close();

    spdlog::info("Wrote chunk {} of file {} (size={})",
                 chunk_request.chunk_index(),
                 dest_path.string(),
                 data.size());
    co_return true;
}

bool SingleFileReceiver::verify_chunk_hash(const transfer::FileChunkRequest& chunk_request) {
    const std::string& data = chunk_request.data();
    ConstDataBlock block(reinterpret_cast<const std::byte*>(data.data()), data.size());
    auto chunk_hash = util::hash::sha256_hex(block);

    if (*chunk_hash != chunk_request.hash()) {
        spdlog::error("Chunk hash mismatch for {} chunk {} (expected {} got {})",
                      file_info_.relative_path(),
                      chunk_request.chunk_index(),
                      chunk_request.hash(),
                      *chunk_hash);
        return false;
    }

    return true;
}

asio::awaitable<bool> SingleFileReceiver::verify_file_hash(const std::filesystem::path& dest_path) {
    auto file_hash_hex = util::hash::sha256_file_hex(dest_path);

    if (!file_hash_hex) {
        spdlog::error("Failed to compute file hash for {}", dest_path.string());
        co_return false;
    }

    if (*file_hash_hex != file_info_.hash()) {
        spdlog::error("File hash mismatch for {} (expected {} got {})",
                      dest_path.string(),
                      file_info_.hash(),
                      *file_hash_hex);
        co_return false;
    }

    spdlog::info("Received complete file: {} (size={})", dest_path.string(), file_info_.size());
    co_return true;
}

asio::awaitable<void> SingleFileReceiver::receive_chunk(transfer::FileChunkRequest& chunk_request) {
    try {
        const std::filesystem::path dest_path = std::filesystem::current_path()
                                                / std::filesystem::path(file_info_.relative_path());

        if (!co_await prepare_file(dest_path)) {
            co_return;
        }
        if (!verify_chunk_hash(chunk_request)) {
            co_return;
        }

        if (!co_await write_chunk_data(dest_path, chunk_request)) {
            co_return;
        }

        if (chunk_request.is_last_chunk()) {
            co_await verify_file_hash(dest_path);
        }
    } catch (const std::exception& e) {
        spdlog::error("Exception in SingleFileReceiver::receive_chunk: {}", e.what());
    }
    co_return;
}
} // namespace receiver