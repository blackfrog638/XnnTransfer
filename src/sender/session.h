#pragma once
#include "asio/awaitable.hpp"
#include "single_file_sender.h"
#include "transfer.pb.h"
#include <core/net/io/session.h>
#include <memory>
#include <optional>
#include <session.pb.h>

namespace sender {

class Session : public core::net::io::Session {
  public:
    template<typename... FilePaths>
    Session(core::Executor& executor, std::string_view host, uint16_t port, FilePaths&&... paths)
        : core::net::io::Session(executor, host, port)
        , paths_{std::filesystem::path(std::forward<FilePaths>(paths))...} {}

    asio::awaitable<void> start() override;

  private:
    asio::awaitable<void> handle_message(const MessageWrapper& message) override;

    void prepare_file_paths();

    std::optional<std::size_t> find_file_index(const std::string& relative_path) const;

    SingleFileSender* find_file_sender(const std::string& relative_path);

    std::vector<std::filesystem::path> paths_;
    std::vector<std::unique_ptr<SingleFileSender>> file_senders_;

    struct FilePath {
        std::filesystem::path relative;
        std::filesystem::path absolute;
        enum class Status { Succeeded, Failed, InProgress } status{Status::InProgress};
        std::size_t file_index; // 索引位置，对应 metadata_request.files(file_index)
    };
    std::vector<FilePath> file_paths_;
    transfer::TransferMetadataRequest metadata_request_;
};
} // namespace sender