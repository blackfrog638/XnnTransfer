#pragma once
#include "asio/awaitable.hpp"
#include "core/net/io/session.h"
#include "transfer.pb.h"
#include <memory>
#include <string>
#include <unordered_map>

// forward
namespace receiver {
class SingleFileReceiver;
}
namespace receiver {
class Session : public core::net::io::Session {
  public:
    Session(core::Executor& executor, uint16_t port)
        : core::net::io::Session(executor, port) {}

    asio::awaitable<void> start() override;

  private:
    asio::awaitable<void> handle_message(const MessageWrapper& message) override;

    asio::awaitable<void> handle_metadata(const transfer::TransferMetadataRequest& request);
    asio::awaitable<void> handle_file_chunk(const transfer::FileChunkRequest& request);

    std::unordered_map<std::string, std::unique_ptr<SingleFileReceiver>> receivers_map_;
};
} // namespace receiver