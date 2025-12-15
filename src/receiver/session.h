#pragma once
#include "asio/awaitable.hpp"
#include "core/net/io/session.h"
#include "single_file_receiver.h"
#include "transfer.pb.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace receiver {
class Session : public core::net::io::Session {
  public:
    Session(core::Executor& executor, uint16_t port, std::string_view save_dir)
        : core::net::io::Session(executor, port)
        , save_dir_(save_dir) {}

    asio::awaitable<void> start() override;

  private:
    asio::awaitable<void> handle_message(const MessageWrapper& message) override;

    asio::awaitable<void> handle_metadata(const transfer::TransferMetadataRequest& request);
    asio::awaitable<void> handle_file_chunk(const transfer::FileChunkRequest& request);

    std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>> receivers_map_;

    size_t completed_files_ = 0;
    struct FilePath {
        std::filesystem::path relative;
        std::filesystem::path absolute;
        enum class Status { Succeeded, Failed, InProgress } status{Status::InProgress};
        std::size_t file_index;
    };
    std::string save_dir_;
    std::vector<FilePath> file_paths_;
};
} // namespace receiver