#pragma once

#include "asio/awaitable.hpp"
#include "core/executor.h"
#include "core/net/io/tcp_sender.h"
#include "sender/single_file_sender.h"
#include "transfer.pb.h"
#include <asio/ip/tcp.hpp>
#include <filesystem>
#include <memory>
#include <util/uuid.h>
#include <utility>
#include <vector>

namespace sender {

class Session {
  public:
    template<typename... FilePaths>
    Session(core::Executor& executor, std::string_view destination, FilePaths&&... filepaths)
        : executor_(executor)
        , destination_(destination)
        , socket_(executor_.get_io_context())
        , tcp_sender_(executor_, socket_, destination, 0) // TODO: add port parameter
        , filepaths_{std::filesystem::path(std::forward<FilePaths>(filepaths))...} {
        session_id_ = util::generate_uuid();
        session_id_view_ = session_id_;
        prepare_filepaths();
        prepare_metadata_request();
    }
    ~Session() = default;

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    asio::awaitable<void> send();

    const transfer::TransferMetadataRequest& metadata_request() const { return metadata_request_; }

    std::string session_id_;
    std::string_view session_id_view_;

  private:
    void prepare_filepaths();
    void prepare_metadata_request();

    core::Executor& executor_;
    std::string_view destination_;
    asio::ip::tcp::socket socket_;
    core::net::io::TcpSender tcp_sender_;
    std::vector<std::filesystem::path> filepaths_;

    struct FilePath {
        std::filesystem::path relative;
        std::filesystem::path absolute;
    };
    std::vector<FilePath> file_paths_;

    transfer::TransferMetadataRequest metadata_request_;
    std::vector<std::unique_ptr<SingleFileSender>> file_senders_;
};

} // namespace sender