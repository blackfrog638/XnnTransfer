#pragma once

#include "core/executor.h"
#include "core/net/io/session.h"
#include "transfer.pb.h"
#include <cstdint>
#include <filesystem>
#include <string>

namespace sender {
class SingleFileSender {
  public:
    SingleFileSender(core::Executor& executor,
                     core::net::io::Session& session,
                     transfer::FileInfoRequest& file,
                     const std::filesystem::path& absolute_path);
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
        Status status = Status::InProgress;
        std::uint64_t offset = 0;
        std::uint32_t size = 0;
        std::string hash;
        bool is_last = false;
    };
    std::vector<ChunkInfo> chunks_;

    core::net::io::Session& session_;
    std::filesystem::path file_path_; // 绝对路径，用于读取文件
    std::string relative_path_;       // 相对路径，用于协议
    std::uint64_t size_;
    std::string hash_;
    std::uint64_t total_chunks_ = 0;
    std::uint64_t completed_chunks_ = 0;
    bool completion_announced_ = false;
};
} // namespace sender