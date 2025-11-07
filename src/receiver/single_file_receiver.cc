#include "single_file_receiver.h"
#include "util/data_block.h"
#include "util/hash.h"
#include <spdlog/spdlog.h>

namespace receiver {

SingleFileReceiver::SingleFileReceiver(std::string relative_path, std::string expected_file_hash)
    : rel_path_(std::move(relative_path))
    , expected_hash_(std::move(expected_file_hash)) {
    try {
        // store under ./received/<relative_path>
        dest_path_ = std::filesystem::current_path() / "received" / rel_path_;
        auto parent = dest_path_.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent)) {
            std::filesystem::create_directories(parent);
        }

        ofs_.open(dest_path_, std::ios::binary | std::ios::trunc);
        if (!ofs_.is_open()) {
            spdlog::error("[SingleFileReceiver] Failed to open {} for writing", dest_path_.string());
            valid_ = false;
            return;
        }

        valid_ = true;
    } catch (const std::exception& e) {
        spdlog::error("[SingleFileReceiver] Exception preparing file {}: {}", rel_path_, e.what());
        valid_ = false;
    }
}

bool SingleFileReceiver::handle_chunk(const transfer::FileChunkRequest& request) {
    if (!valid_)
        return false;

    // enforce sequential chunks
    if (request.chunk_index() != static_cast<uint64_t>(next_chunk_index_)) {
        spdlog::error("[SingleFileReceiver] Unexpected chunk index for {}: got {}, expected {}",
                      rel_path_,
                      request.chunk_index(),
                      next_chunk_index_);
        return false;
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
                request.chunk_index(),
                request.hash(),
                hex ? *hex : std::string("(none)"));
            return false;
        }
    }

    // write data
    ofs_.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(data.size()));
    if (!ofs_) {
        spdlog::error("[SingleFileReceiver] Failed to write chunk {} for {}",
                      request.chunk_index(),
                      rel_path_);
        return false;
    }

    ++next_chunk_index_;

    if (request.is_last_chunk()) {
        ofs_.flush();
    }

    return true;
}

std::tuple<bool, std::string, std::string> SingleFileReceiver::finalize_and_verify() {
    if (ofs_.is_open()) {
        ofs_.close();
    }

    std::optional<std::string> actual = util::hash::sha256_file_hex(dest_path_);
    std::string act = actual ? *actual : std::string();
    bool ok = true;
    if (!expected_hash_.empty()) {
        ok = (actual.has_value() && *actual == expected_hash_);
    }

    return {ok, expected_hash_, act};
}

} // namespace receiver
