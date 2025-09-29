#pragma once

#include "core/acceptor.h"
#include "core/executor.h"
#include "core/io/data_block.h"
#include <asio/ip/tcp.hpp>
#include <cstdint>

namespace core::net::io {
class TcpReceiver {
  public:
    TcpReceiver(Executor& executor, asio::ip::tcp::socket& socket, std::uint16_t port);
    ~TcpReceiver() = default;

    TcpReceiver(const TcpReceiver&) = delete;
    TcpReceiver& operator=(const TcpReceiver&) = delete;

    asio::awaitable<void> receive(MutDataBlock& buffer);

  private:
    Executor& executor_;
    asio::ip::tcp::socket& socket_;
    core::net::Acceptor acceptor_;
};
} //namespace core::net::io