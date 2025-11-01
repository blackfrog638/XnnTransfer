#pragma once

#include "asio/awaitable.hpp"
#include "core/executor.h"
#include "core/net/io/tcp_receiver.h"
#include "single_file_receiver.h"
#include "transfer.pb.h"
#include <asio/ip/tcp.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace receiver {

constexpr uint16_t kListeningPort = 14648;

class Session {
  public:
    explicit Session(core::Executor& executor);
    virtual ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    asio::awaitable<void> receive();

  private:
    asio::awaitable<std::optional<transfer::TransferMetadataRequest>> receive_metadata(
        std::vector<std::byte>& buffer);

    std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>> create_receivers(
        const transfer::TransferMetadataRequest& metadata,
        std::vector<transfer::FileInfoRequest>& file_infos);

    asio::awaitable<std::optional<transfer::FileChunkRequest>> receive_chunk(
        std::vector<std::byte>& buffer);

    void dispatch_chunk(
        const transfer::FileChunkRequest& chunk,
        std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>>& receivers);

    asio::awaitable<void> send_metadata_response(const transfer::TransferMetadataRequest& metadata,
                                                 transfer::TransferMetadataResponse::Status status);

    core::Executor& executor_;
    std::string_view session_id_;
    asio::ip::tcp::socket socket_;
    core::net::io::TcpReceiver tcp_receiver_;
};
} // namespace receiver