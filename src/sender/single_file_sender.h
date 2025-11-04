#pragma once

#include "core/executor.h"
#include "core/net/io/session.h"
#include "transfer.pb.h"
#include <cstdint>
#include <filesystem>

namespace sender {
class SingleFileSender {
  public:
    SingleFileSender(core::Executor& executor,
                     core::net::io::Session& session,
                     transfer::FileInfoRequest& file);
    ~SingleFileSender() = default;

    SingleFileSender(const SingleFileSender&) = delete;
    SingleFileSender& operator=(const SingleFileSender&) = delete;

    asio::awaitable<void> send_file();
    asio::awaitable<void> send_chunk(std::uint64_t chunk_index);

    void update_chunk_status(std::uint64_t chunk_index, bool success);

  private:
    core::Executor& executor_;
    struct ChunkInfo {
        enum class Status { InProgress, Completed, Failed };
        Status status;
        std::uint32_t offset;
        std::uint32_t size;
    };
    std::vector<ChunkInfo> chunks_;

    core::net::io::Session& session_;
    std::filesystem::path file_path_;
    std::uint64_t size_;
    std::string hash_;
};
} // namespace sender