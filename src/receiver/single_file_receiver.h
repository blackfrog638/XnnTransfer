#pragma once
#include "asio/awaitable.hpp"
#include "core/executor.h"
#include "core/net/io/tcp_receiver.h"
#include "transfer.pb.h"
#include <filesystem>

namespace receiver {

class SingleFileReceiver {
  public:
    SingleFileReceiver(core::Executor& executor,
                       transfer::FileInfoRequest& file_info,
                       core::net::io::TcpReceiver& tcp_receiver);
    ~SingleFileReceiver() = default;

    SingleFileReceiver(const SingleFileReceiver&) = delete;
    SingleFileReceiver& operator=(const SingleFileReceiver&) = delete;

    asio::awaitable<void> receive_chunk(transfer::FileChunkRequest& chunk_request);

  private:
    asio::awaitable<bool> receive_chunk_impl(transfer::FileChunkRequest& chunk_request);

    static asio::awaitable<bool> prepare_file(const std::filesystem::path& dest_path);

    static asio::awaitable<bool> write_chunk_data(const std::filesystem::path& dest_path,
                                                  const transfer::FileChunkRequest& chunk_request);

    bool verify_chunk_hash(const transfer::FileChunkRequest& chunk_request);

    asio::awaitable<bool> verify_file_hash(const std::filesystem::path& dest_path);

    asio::awaitable<void> send_chunk_response(const transfer::FileChunkRequest& chunk_request,
                                              transfer::FileChunkResponse::Status status);

    asio::awaitable<void> send_file_complete_response(const transfer::FileInfoRequest& file_info,
                                                      transfer::FileInfoResponse::Status status);

    core::Executor& executor_;
    transfer::FileInfoRequest file_info_;
    core::net::io::TcpReceiver& tcp_receiver_;
};

} //namespace receiver