#pragma once

#include "transfer.pb.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <vector>

namespace receiver {

class SingleFileReceiver {
  public:
    SingleFileReceiver(std::string relative_path,
                       std::string expected_file_hash,
                       std::uint64_t file_size = 0);

    const std::string& relative_path() const { return rel_path_; }
    const std::filesystem::path& destination_path() const { return dest_path_; }
    bool prepare_storage(const std::filesystem::path& dest_path);
    bool is_ready() const { return fs_.is_open(); }
    bool is_valid() const { return true; } // Receiver is always valid after construction
    bool is_complete() const;
    std::uint64_t completed_chunks() const { return completed_chunks_; }

    bool handle_chunk(const transfer::FileChunkRequest& request);

    std::tuple<bool, std::string, std::string> finalize_and_verify();

  private:
    std::string rel_path_;
    std::filesystem::path dest_path_;
    std::fstream fs_;
    std::uint64_t completed_chunks_ = 0;

    struct ChunkInfo {
        enum class Status { InProgress, Completed, Failed };
        Status status = Status::InProgress;
        std::uint64_t offset = 0;
        std::uint32_t size = 0;
        std::string hash;
        bool is_last = false;
    };
    std::vector<ChunkInfo> chunks_;

    std::uint64_t expected_total_chunks_ = 0;
    std::uint64_t file_size_ = 0;
    std::string expected_hash_;
    bool storage_prepared_ = false;
    std::uint64_t bytes_received_ = 0;
    bool last_chunk_received_ = false;
    bool finalized_ = false;
};

} // namespace receiver
