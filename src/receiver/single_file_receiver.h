#pragma once

#include "transfer.pb.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <tuple>

namespace receiver {

class SingleFileReceiver {
  public:
    SingleFileReceiver(std::string relative_path, std::string expected_file_hash);

    bool is_valid() const { return valid_; }
    const std::string& relative_path() const { return rel_path_; }

    bool handle_chunk(const transfer::FileChunkRequest& request);

    std::tuple<bool, std::string, std::string> finalize_and_verify();

  private:
    std::string rel_path_;
    std::filesystem::path dest_path_;
    std::ofstream ofs_;
    std::size_t next_chunk_index_ = 0;
    std::string expected_hash_;
    bool valid_ = false;
};

} // namespace receiver
