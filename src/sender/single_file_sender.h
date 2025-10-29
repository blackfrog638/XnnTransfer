#pragma once

#include "asio/awaitable.hpp"
#include "core/executor.h"
#include "core/net/io/tcp_sender.h"
#include "transfer.pb.h"
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sender {
constexpr uint64_t kChunkSize = 1 * 1024 * 1024;
constexpr uint16_t kTargetPort = 14648;

enum class ChunkStatus { Waiting, Reading, Pending, Sending, Sent, Failed };

class SingleFileSender {
  public:
    SingleFileSender(core::Executor& executor,
                     core::net::io::TcpSender& tcp_sender,
                     std::string_view& session_id,
                     std::filesystem::path& file_path,
                     std::filesystem::path& relative_path);
    ~SingleFileSender() = default;

    SingleFileSender(const SingleFileSender&) = delete;
    SingleFileSender& operator=(const SingleFileSender&) = delete;

    asio::awaitable<void> send();

    const std::vector<ChunkStatus>& status() const { return status_; }

    transfer::FileInfoRequest& file_info() { return file_info_; }

  private:
    struct ChunkData {
        std::string payload;
        std::size_t size = 0;
    };
    std::optional<ChunkData> load_chunk(std::ifstream& input,
                                        std::vector<std::byte>& buffer,
                                        uint64_t index,
                                        std::string_view& session_id,
                                        bool is_last_chunk);

    asio::awaitable<void> send_chunk(const ChunkData& chunk, uint64_t index);

    std::string_view& session_id_;
    core::Executor& executor_;
    core::net::io::TcpSender& tcp_sender_;
    std::filesystem::path& relative_path_;
    std::filesystem::path& file_path_;

    transfer::FileInfoRequest file_info_;

    uint64_t file_size_ = 0;
    uint64_t bytes_sent_ = 0;
    uint64_t chunks_count_ = 0;

    std::vector<ChunkStatus> status_;
};
} // namespace sender