#pragma once

#include "transfer.pb.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>
#include <unordered_set>

namespace receiver {

class SingleFileReceiver {
  public:
    SingleFileReceiver(std::string relative_path,
                       std::string expected_file_hash,
                       std::uint64_t file_size = 0);

    bool is_valid() const { return valid_; }
    const std::string& relative_path() const { return rel_path_; }

    bool handle_chunk(const transfer::FileChunkRequest& request);

    std::tuple<bool, std::string, std::string> finalize_and_verify();

  private:
    std::string rel_path_;
    std::filesystem::path dest_path_;
    std::fstream fs_;
    std::unordered_set<std::uint64_t> received_chunks_;
    std::uint64_t expected_total_chunks_ = 0;
    std::uint64_t file_size_ = 0;
    std::string expected_hash_;
    bool valid_ = false;
    bool last_chunk_received_ = false;
};

} // namespace receiver
