#pragma once
#include "asio/awaitable.hpp"
#include "transfer.pb.h"
#include <filesystem>

namespace receiver {

class SingleFileReceiver {
  public:
    explicit SingleFileReceiver(transfer::FileInfoRequest& file_info);
    ~SingleFileReceiver() = default;

    SingleFileReceiver(const SingleFileReceiver&) = delete;
    SingleFileReceiver& operator=(const SingleFileReceiver&) = delete;

    asio::awaitable<void> receive_chunk(transfer::FileChunkRequest& chunk_request);

  private:
    // 准备文件目录和文件
    static asio::awaitable<bool> prepare_file(const std::filesystem::path& dest_path);

    // 写入文件块数据
    static asio::awaitable<bool> write_chunk_data(const std::filesystem::path& dest_path,
                                                  const transfer::FileChunkRequest& chunk_request);

    // 验证文件块哈希
    bool verify_chunk_hash(const transfer::FileChunkRequest& chunk_request);

    // 验证完整文件哈希
    asio::awaitable<bool> verify_file_hash(const std::filesystem::path& dest_path);

    transfer::FileInfoRequest file_info_;
};

} //namespace receiver