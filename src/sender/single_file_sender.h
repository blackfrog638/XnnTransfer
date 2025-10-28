#pragma once
#include "asio/awaitable.hpp"
#include "core/executor.h"
#include "core/net/io/tcp_sender.h"
#include <asio/ip/tcp.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace sender {
constexpr uint64_t chunk_size_ = 1 * 1024 * 1024;

enum class ChunkStatus { Waiting, Reading, Pending, Sending, Sent, Failed };

class SingleFileSender {
  public:
    SingleFileSender(core::Executor& executor,
                     const std::filesystem::path& file_path,
                     std::string_view target_address,
                     uint16_t target_port);
    ~SingleFileSender() = default;

    SingleFileSender(const SingleFileSender&) = delete;
    SingleFileSender& operator=(const SingleFileSender&) = delete;

    asio::awaitable<void> send();

    const std::vector<ChunkStatus>& status() const { return status_; }

  private:
    struct ChunkData {
        std::string payload;
        std::size_t size = 0;
    };

    std::string_view target_address_;
    uint16_t target_port_;
    core::Executor& executor_;
    core::net::io::TcpSender tcp_sender_;
    std::filesystem::path file_path_;
    asio::ip::tcp::socket socket_;

    uint64_t file_size_ = 0;
    uint64_t bytes_sent_ = 0;
    uint64_t chunks_count_ = 0;

    std::vector<ChunkStatus> status_;

    std::optional<ChunkData> load_chunk(std::ifstream& input,
                                        std::vector<std::byte>& buffer,
                                        uint64_t index,
                                        const std::string& session_id,
                                        bool is_last_chunk);

    asio::awaitable<void> send_chunk(const ChunkData& chunk, uint64_t index);
};
} // namespace sender